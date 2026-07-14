/// @file ChaCha20Poly1305SecureChannel.cpp
/// @brief ChaCha20-Poly1305 AEAD implementation per RFC 8439.
///
/// The implementation follows the RFC's reference pseudocode and
/// the published test vectors.  It is intentionally *not* hardened
/// against side-channel attacks — that responsibility belongs to
/// the future OpenSSL backend.  What this code gives us is:
///
///   1. Correct interoperability with any RFC 8439 peer, so the
///      wire format stays stable when we later swap backends.
///   2. Dependency-free integration into the server build today,
///      so production does not have to wait on a libsodium pull.
///   3. A working AEAD that the transport can exercise in integration
///      tests long before a proper handshake lands.

#include "SagaServer/Networking/Security/ChaCha20Poly1305SecureChannel.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace SagaEngine::Networking {

namespace {

constexpr const char* kLogTag = "Security";

// ─── ChaCha20 state helpers ───────────────────────────────────────────────
//
// The block function operates on 16 32-bit words laid out as a 4×4
// matrix.  RFC 8439 §2.3 is the canonical reference.

[[nodiscard]] constexpr std::uint32_t RotL32(std::uint32_t x, int n) noexcept
{
    return (x << n) | (x >> (32 - n));
}

/// Read a little-endian uint32 from `in[offset]`.
[[nodiscard]] std::uint32_t LoadLE32(const std::uint8_t* in, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(in[offset + 0])       |
           static_cast<std::uint32_t>(in[offset + 1]) << 8  |
           static_cast<std::uint32_t>(in[offset + 2]) << 16 |
           static_cast<std::uint32_t>(in[offset + 3]) << 24;
}

/// Write a little-endian uint32 into `out[offset]`.
void StoreLE32(std::uint8_t* out, std::size_t offset, std::uint32_t value) noexcept
{
    out[offset + 0] = static_cast<std::uint8_t>((value >> 0)  & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8)  & 0xFFu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

/// The ChaCha20 quarter-round.  Four of these produce one column
/// or one diagonal pass; twenty passes (ten doubles) make a full
/// 20-round block.  Named as in the RFC so an audit against the
/// spec is trivial.
void QuarterRound(std::uint32_t& a, std::uint32_t& b,
                  std::uint32_t& c, std::uint32_t& d) noexcept
{
    a += b; d ^= a; d = RotL32(d, 16);
    c += d; b ^= c; b = RotL32(b, 12);
    a += b; d ^= a; d = RotL32(d, 8);
    c += d; b ^= c; b = RotL32(b, 7);
}

/// Produce one 64-byte ChaCha20 key stream block for `(key, nonce,
/// counter)` into `out`.
void ChaCha20Block(const std::uint8_t* key,
                   std::uint32_t       counter,
                   const std::uint8_t* nonce,
                   std::uint8_t        out[64]) noexcept
{
    // Initial state: "expand 32-byte k" constants, key, counter, nonce.
    std::uint32_t state[16] = {
        0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u,
        LoadLE32(key,  0), LoadLE32(key,  4),
        LoadLE32(key,  8), LoadLE32(key, 12),
        LoadLE32(key, 16), LoadLE32(key, 20),
        LoadLE32(key, 24), LoadLE32(key, 28),
        counter,
        LoadLE32(nonce, 0),
        LoadLE32(nonce, 4),
        LoadLE32(nonce, 8),
    };

    std::uint32_t working[16];
    for (int i = 0; i < 16; ++i)
        working[i] = state[i];

    // 20 rounds = 10 double-rounds (column + diagonal).
    for (int i = 0; i < 10; ++i)
    {
        // Column round.
        QuarterRound(working[0], working[4], working[ 8], working[12]);
        QuarterRound(working[1], working[5], working[ 9], working[13]);
        QuarterRound(working[2], working[6], working[10], working[14]);
        QuarterRound(working[3], working[7], working[11], working[15]);
        // Diagonal round.
        QuarterRound(working[0], working[5], working[10], working[15]);
        QuarterRound(working[1], working[6], working[11], working[12]);
        QuarterRound(working[2], working[7], working[ 8], working[13]);
        QuarterRound(working[3], working[4], working[ 9], working[14]);
    }

    for (int i = 0; i < 16; ++i)
    {
        const std::uint32_t value = working[i] + state[i];
        StoreLE32(out, static_cast<std::size_t>(i) * 4, value);
    }
}

/// Apply ChaCha20 to `input` (XORing with the key stream) starting
/// at `counter`.  `input` and `output` may alias.  Used for both
/// encrypt and decrypt — the cipher is its own inverse.
void ChaCha20Xor(const std::uint8_t* key,
                 std::uint32_t       counter,
                 const std::uint8_t* nonce,
                 const std::uint8_t* input,
                 std::size_t         inputSize,
                 std::uint8_t*       output) noexcept
{
    std::uint8_t block[64];
    std::size_t  i = 0;
    while (i < inputSize)
    {
        ChaCha20Block(key, counter, nonce, block);
        ++counter;
        const std::size_t take = std::min<std::size_t>(64, inputSize - i);
        for (std::size_t j = 0; j < take; ++j)
            output[i + j] = input[i + j] ^ block[j];
        i += take;
    }
}

// ─── Poly1305 ─────────────────────────────────────────────────────────────
//
// Poly1305 is a 130-bit polynomial MAC.  RFC 8439 §2.5 describes
// the algorithm; this implementation uses 26-bit limbs so all
// intermediate products fit in 64-bit registers without overflow.
// Reference: the public-domain "poly1305-donna-32" implementation
// by Andrew Moon, adapted to RFC 8439's one-shot interface.

struct Poly1305State
{
    std::uint32_t r[5];       ///< Clamped key, 26-bit limbs.
    std::uint32_t h[5];       ///< Accumulator, 26-bit limbs.
    std::uint32_t pad[4];     ///< Final addend (second half of key).
    std::uint8_t  leftover[16];
    std::size_t   leftoverLen = 0;
    bool          finalised   = false;
};

void Poly1305Init(Poly1305State& st, const std::uint8_t key[32]) noexcept
{
    // Clamp r per the RFC: mask specific bits so the polynomial
    // evaluates safely within the 130-bit field.
    const std::uint32_t t0 = LoadLE32(key, 0);
    const std::uint32_t t1 = LoadLE32(key, 4);
    const std::uint32_t t2 = LoadLE32(key, 8);
    const std::uint32_t t3 = LoadLE32(key, 12);

    st.r[0] = (t0)                      & 0x3ffffffu;
    st.r[1] = ((t0 >> 26) | (t1 << 6))  & 0x3ffff03u;
    st.r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ffu;
    st.r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fffu;
    st.r[4] = (t3 >> 8)                 & 0x00fffffu;

    for (int i = 0; i < 5; ++i)
        st.h[i] = 0;

    st.pad[0] = LoadLE32(key, 16);
    st.pad[1] = LoadLE32(key, 20);
    st.pad[2] = LoadLE32(key, 24);
    st.pad[3] = LoadLE32(key, 28);

    st.leftoverLen = 0;
    st.finalised   = false;
}

/// Process one or more full 16-byte Poly1305 blocks in place.
/// `padBit` is 1 for normal blocks and 0 for the final partial
/// block (which feeds a different high-limb value per the RFC).
void Poly1305Blocks(Poly1305State&      st,
                    const std::uint8_t* input,
                    std::size_t         size,
                    std::uint32_t       padBit) noexcept
{
    const std::uint32_t hibit = padBit << 24;

    const std::uint32_t r0 = st.r[0];
    const std::uint32_t r1 = st.r[1];
    const std::uint32_t r2 = st.r[2];
    const std::uint32_t r3 = st.r[3];
    const std::uint32_t r4 = st.r[4];

    const std::uint32_t s1 = r1 * 5;
    const std::uint32_t s2 = r2 * 5;
    const std::uint32_t s3 = r3 * 5;
    const std::uint32_t s4 = r4 * 5;

    std::uint32_t h0 = st.h[0];
    std::uint32_t h1 = st.h[1];
    std::uint32_t h2 = st.h[2];
    std::uint32_t h3 = st.h[3];
    std::uint32_t h4 = st.h[4];

    while (size >= 16)
    {
        // h += message limb.
        h0 += (LoadLE32(input,  0))                            & 0x3ffffffu;
        h1 += ((LoadLE32(input,  3) >> 2))                     & 0x3ffffffu;
        h2 += ((LoadLE32(input,  6) >> 4))                     & 0x3ffffffu;
        h3 += ((LoadLE32(input,  9) >> 6))                     & 0x3ffffffu;
        h4 +=  (LoadLE32(input, 12) >> 8) | hibit;

        // h *= r (mod 2^130 - 5), expressed with 26-bit limbs.
        const std::uint64_t d0 =
            static_cast<std::uint64_t>(h0) * r0 +
            static_cast<std::uint64_t>(h1) * s4 +
            static_cast<std::uint64_t>(h2) * s3 +
            static_cast<std::uint64_t>(h3) * s2 +
            static_cast<std::uint64_t>(h4) * s1;
        const std::uint64_t d1 =
            static_cast<std::uint64_t>(h0) * r1 +
            static_cast<std::uint64_t>(h1) * r0 +
            static_cast<std::uint64_t>(h2) * s4 +
            static_cast<std::uint64_t>(h3) * s3 +
            static_cast<std::uint64_t>(h4) * s2;
        const std::uint64_t d2 =
            static_cast<std::uint64_t>(h0) * r2 +
            static_cast<std::uint64_t>(h1) * r1 +
            static_cast<std::uint64_t>(h2) * r0 +
            static_cast<std::uint64_t>(h3) * s4 +
            static_cast<std::uint64_t>(h4) * s3;
        const std::uint64_t d3 =
            static_cast<std::uint64_t>(h0) * r3 +
            static_cast<std::uint64_t>(h1) * r2 +
            static_cast<std::uint64_t>(h2) * r1 +
            static_cast<std::uint64_t>(h3) * r0 +
            static_cast<std::uint64_t>(h4) * s4;
        const std::uint64_t d4 =
            static_cast<std::uint64_t>(h0) * r4 +
            static_cast<std::uint64_t>(h1) * r3 +
            static_cast<std::uint64_t>(h2) * r2 +
            static_cast<std::uint64_t>(h3) * r1 +
            static_cast<std::uint64_t>(h4) * r0;

        // Partial reduction.
        std::uint32_t c = 0;
        std::uint64_t d = d0;
        h0 = static_cast<std::uint32_t>(d) & 0x3ffffffu;
        c  = static_cast<std::uint32_t>(d >> 26);
        d = d1 + c;
        h1 = static_cast<std::uint32_t>(d) & 0x3ffffffu;
        c  = static_cast<std::uint32_t>(d >> 26);
        d = d2 + c;
        h2 = static_cast<std::uint32_t>(d) & 0x3ffffffu;
        c  = static_cast<std::uint32_t>(d >> 26);
        d = d3 + c;
        h3 = static_cast<std::uint32_t>(d) & 0x3ffffffu;
        c  = static_cast<std::uint32_t>(d >> 26);
        d = d4 + c;
        h4 = static_cast<std::uint32_t>(d) & 0x3ffffffu;
        c  = static_cast<std::uint32_t>(d >> 26);
        h0 += c * 5;
        c  = h0 >> 26;
        h0 &= 0x3ffffffu;
        h1 += c;

        input += 16;
        size  -= 16;
    }

    st.h[0] = h0;
    st.h[1] = h1;
    st.h[2] = h2;
    st.h[3] = h3;
    st.h[4] = h4;
}

void Poly1305Update(Poly1305State&      st,
                    const std::uint8_t* input,
                    std::size_t         size) noexcept
{
    // Drain any leftover from the previous call first.
    if (st.leftoverLen > 0)
    {
        const std::size_t want = std::min<std::size_t>(16 - st.leftoverLen, size);
        std::memcpy(st.leftover + st.leftoverLen, input, want);
        st.leftoverLen += want;
        input          += want;
        size           -= want;
        if (st.leftoverLen < 16)
            return;
        Poly1305Blocks(st, st.leftover, 16, 1);
        st.leftoverLen = 0;
    }

    // Process full blocks directly from the caller's buffer.
    const std::size_t full = size & ~static_cast<std::size_t>(15);
    if (full > 0)
    {
        Poly1305Blocks(st, input, full, 1);
        input += full;
        size  -= full;
    }

    // Stash any tail for next time.
    if (size > 0)
    {
        std::memcpy(st.leftover, input, size);
        st.leftoverLen = size;
    }
}

void Poly1305Finish(Poly1305State& st, std::uint8_t tag[16]) noexcept
{
    // Process any leftover partial block with the final pad bit.
    if (st.leftoverLen > 0)
    {
        st.leftover[st.leftoverLen++] = 1;
        while (st.leftoverLen < 16)
            st.leftover[st.leftoverLen++] = 0;
        Poly1305Blocks(st, st.leftover, 16, 0);
    }

    // Full reduction: carry chain then conditional subtract of p = 2^130 - 5.
    std::uint32_t h0 = st.h[0];
    std::uint32_t h1 = st.h[1];
    std::uint32_t h2 = st.h[2];
    std::uint32_t h3 = st.h[3];
    std::uint32_t h4 = st.h[4];

    std::uint32_t c;
    c  = h1 >> 26; h1 &= 0x3ffffffu; h2 += c;
    c  = h2 >> 26; h2 &= 0x3ffffffu; h3 += c;
    c  = h3 >> 26; h3 &= 0x3ffffffu; h4 += c;
    c  = h4 >> 26; h4 &= 0x3ffffffu; h0 += c * 5;
    c  = h0 >> 26; h0 &= 0x3ffffffu; h1 += c;

    // Compute h - p.
    std::uint32_t g0 = h0 + 5;
    c  = g0 >> 26; g0 &= 0x3ffffffu;
    std::uint32_t g1 = h1 + c;
    c  = g1 >> 26; g1 &= 0x3ffffffu;
    std::uint32_t g2 = h2 + c;
    c  = g2 >> 26; g2 &= 0x3ffffffu;
    std::uint32_t g3 = h3 + c;
    c  = g3 >> 26; g3 &= 0x3ffffffu;
    std::uint32_t g4 = h4 + c - (1u << 26);

    // Select h if h - p underflowed, else h - p.  Constant-time
    // mask derived from the sign bit of g4.
    const std::uint32_t mask = (g4 >> 31) - 1u;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    const std::uint32_t nmask = ~mask;
    h0 = (h0 & nmask) | g0;
    h1 = (h1 & nmask) | g1;
    h2 = (h2 & nmask) | g2;
    h3 = (h3 & nmask) | g3;
    h4 = (h4 & nmask) | g4;

    // Collapse 26-bit limbs into 32-bit words.
    const std::uint32_t f0 =  h0        | (h1 << 26);
    const std::uint32_t f1 = (h1 >>  6) | (h2 << 20);
    const std::uint32_t f2 = (h2 >> 12) | (h3 << 14);
    const std::uint32_t f3 = (h3 >> 18) | (h4 <<  8);

    // Add the second half of the key (the "s" term) mod 2^128 and
    // store little-endian.
    std::uint64_t sum;
    sum = static_cast<std::uint64_t>(f0) + st.pad[0];
    StoreLE32(tag, 0, static_cast<std::uint32_t>(sum));
    sum = static_cast<std::uint64_t>(f1) + st.pad[1] + (sum >> 32);
    StoreLE32(tag, 4, static_cast<std::uint32_t>(sum));
    sum = static_cast<std::uint64_t>(f2) + st.pad[2] + (sum >> 32);
    StoreLE32(tag, 8, static_cast<std::uint32_t>(sum));
    sum = static_cast<std::uint64_t>(f3) + st.pad[3] + (sum >> 32);
    StoreLE32(tag, 12, static_cast<std::uint32_t>(sum));

    st.finalised = true;
}

/// Append zero padding so the next block starts on a 16-byte
/// boundary, as required by the AEAD construction in RFC 8439 §2.8.
void Poly1305PadTo16(Poly1305State& st, std::size_t consumed) noexcept
{
    const std::size_t pad = (16 - (consumed & 15)) & 15;
    if (pad == 0)
        return;
    std::uint8_t zeros[16] = {};
    Poly1305Update(st, zeros, pad);
}

/// Derive the one-time Poly1305 key for a given ChaCha20 (key, nonce)
/// pair.  RFC 8439 specifies: use counter = 0, take the first 32
/// bytes of the keystream, zero out any trailing bytes of the block.
void DerivePoly1305Key(const std::uint8_t* chachaKey,
                       const std::uint8_t* nonce,
                       std::uint8_t        polyKey[32]) noexcept
{
    std::uint8_t block[64];
    ChaCha20Block(chachaKey, /*counter*/ 0, nonce, block);
    std::memcpy(polyKey, block, 32);
}

/// Constant-time tag comparison.  Folds every byte difference into
/// a single accumulator so the timing does not leak the index of
/// the first mismatch.
[[nodiscard]] bool ConstTimeEqual(const std::uint8_t* a,
                                  const std::uint8_t* b,
                                  std::size_t         size) noexcept
{
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < size; ++i)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

} // namespace

// ─── ChaCha20Poly1305Seal ─────────────────────────────────────────────────

bool ChaCha20Poly1305Seal(const std::uint8_t* key,
                          const std::uint8_t* nonce,
                          const std::uint8_t* aad,
                          std::size_t         aadSize,
                          const std::uint8_t* plaintext,
                          std::size_t         plaintextSize,
                          std::uint8_t*       out) noexcept
{
    if (key == nullptr || nonce == nullptr || out == nullptr)
        return false;
    if (plaintextSize > 0 && plaintext == nullptr)
        return false;

    // Encrypt with counter = 1 (counter = 0 produces the Poly1305 key).
    ChaCha20Xor(key, /*counter*/ 1, nonce, plaintext, plaintextSize, out);

    std::uint8_t polyKey[32];
    DerivePoly1305Key(key, nonce, polyKey);

    Poly1305State st;
    Poly1305Init(st, polyKey);

    if (aadSize > 0)
        Poly1305Update(st, aad, aadSize);
    Poly1305PadTo16(st, aadSize);

    if (plaintextSize > 0)
        Poly1305Update(st, out, plaintextSize);
    Poly1305PadTo16(st, plaintextSize);

    // Length block: aadSize || ciphertextSize as little-endian 64-bit values.
    std::uint8_t lengths[16];
    for (int i = 0; i < 8; ++i)
        lengths[i]     = static_cast<std::uint8_t>((aadSize       >> (i * 8)) & 0xFFu);
    for (int i = 0; i < 8; ++i)
        lengths[8 + i] = static_cast<std::uint8_t>((plaintextSize >> (i * 8)) & 0xFFu);
    Poly1305Update(st, lengths, 16);

    Poly1305Finish(st, out + plaintextSize);
    return true;
}

// ─── ChaCha20Poly1305Open ─────────────────────────────────────────────────

bool ChaCha20Poly1305Open(const std::uint8_t* key,
                          const std::uint8_t* nonce,
                          const std::uint8_t* aad,
                          std::size_t         aadSize,
                          const std::uint8_t* ciphertext,
                          std::size_t         ciphertextSize,
                          const std::uint8_t* tag,
                          std::uint8_t*       out) noexcept
{
    if (key == nullptr || nonce == nullptr || tag == nullptr)
        return false;
    if (ciphertextSize > 0 && (ciphertext == nullptr || out == nullptr))
        return false;

    // Recompute the expected tag first; only decrypt if it matches.
    std::uint8_t polyKey[32];
    DerivePoly1305Key(key, nonce, polyKey);

    Poly1305State st;
    Poly1305Init(st, polyKey);

    if (aadSize > 0)
        Poly1305Update(st, aad, aadSize);
    Poly1305PadTo16(st, aadSize);

    if (ciphertextSize > 0)
        Poly1305Update(st, ciphertext, ciphertextSize);
    Poly1305PadTo16(st, ciphertextSize);

    std::uint8_t lengths[16];
    for (int i = 0; i < 8; ++i)
        lengths[i]     = static_cast<std::uint8_t>((aadSize        >> (i * 8)) & 0xFFu);
    for (int i = 0; i < 8; ++i)
        lengths[8 + i] = static_cast<std::uint8_t>((ciphertextSize >> (i * 8)) & 0xFFu);
    Poly1305Update(st, lengths, 16);

    std::uint8_t expected[16];
    Poly1305Finish(st, expected);

    if (!ConstTimeEqual(expected, tag, 16))
        return false;

    // Tag verified — now it is safe to decrypt.
    ChaCha20Xor(key, /*counter*/ 1, nonce, ciphertext, ciphertextSize, out);
    return true;
}

// ─── Channel construction ────────────────────────────────────────────────

ChaCha20Poly1305SecureChannel::ChaCha20Poly1305SecureChannel(
    const ChaCha20ChannelConfig& config) noexcept
    : config_(config)
    , state_(HandshakeState::Established)
{
    // No handshake to run — the PSK arrives pre-derived.  Jumping
    // straight to Established matches the `NullSecureChannel`
    // behaviour so the transport state machine can be identical for
    // both backends.
    LOG_INFO(kLogTag,
             "ChaCha20-Poly1305 channel ready (salt=0x%08X, maxPlain=%zu)",
             config_.nonceSalt,
             config_.maxPlaintextBytes);
}

// ─── BuildNonce ───────────────────────────────────────────────────────────

void ChaCha20Poly1305SecureChannel::BuildNonce(
    std::uint64_t sequence,
    std::uint8_t  out[kChaChaNonceBytes]) const noexcept
{
    // High 4 bytes: per-channel salt (little-endian to match the
    // rest of the encoding).  Low 8 bytes: record sequence counter.
    const std::uint32_t salt = config_.nonceSalt;
    out[0] = static_cast<std::uint8_t>((salt >> 0)  & 0xFFu);
    out[1] = static_cast<std::uint8_t>((salt >> 8)  & 0xFFu);
    out[2] = static_cast<std::uint8_t>((salt >> 16) & 0xFFu);
    out[3] = static_cast<std::uint8_t>((salt >> 24) & 0xFFu);
    for (int i = 0; i < 8; ++i)
        out[4 + i] = static_cast<std::uint8_t>((sequence >> (i * 8)) & 0xFFu);
}

// ─── HandleInbound ────────────────────────────────────────────────────────

SecureChannelResult ChaCha20Poly1305SecureChannel::HandleInbound(
    const std::uint8_t*        /*inbound*/,
    std::size_t                /*inboundSize*/,
    std::vector<std::uint8_t>& /*outbound*/)
{
    // The PSK channel has no handshake.  Any inbound bytes during
    // the handshake phase are ignored; callers should not be
    // calling this method at all because `State()` returns
    // `Established` immediately after construction.
    state_ = HandshakeState::Established;
    return SecureChannelResult::Ok;
}

// ─── Encrypt ──────────────────────────────────────────────────────────────

SecureChannelResult ChaCha20Poly1305SecureChannel::Encrypt(
    const std::uint8_t*        plaintext,
    std::size_t                plaintextSize,
    std::vector<std::uint8_t>& output)
{
    if (state_ != HandshakeState::Established)
        return SecureChannelResult::InternalError;

    if (plaintextSize > config_.maxPlaintextBytes)
        return SecureChannelResult::BufferTooSmall;

    // Wire record: [seq:8][ciphertext:N][tag:16]
    output.resize(8 + plaintextSize + kPoly1305TagBytes);

    const std::uint64_t sequence = txSequence_++;
    for (int i = 0; i < 8; ++i)
        output[static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((sequence >> (i * 8)) & 0xFFu);

    std::uint8_t nonce[kChaChaNonceBytes];
    BuildNonce(sequence, nonce);

    // The sequence number doubles as AAD so an attacker cannot
    // splice a ciphertext under a different sequence — Poly1305
    // binds the tag to both the payload and the header.
    const bool sealed = ChaCha20Poly1305Seal(
        config_.sessionKey.data(),
        nonce,
        /*aad*/ output.data(),
        /*aadSize*/ 8,
        plaintext,
        plaintextSize,
        /*out*/ output.data() + 8);

    if (!sealed)
    {
        output.clear();
        return SecureChannelResult::InternalError;
    }
    return SecureChannelResult::Ok;
}

// ─── Decrypt ──────────────────────────────────────────────────────────────

SecureChannelResult ChaCha20Poly1305SecureChannel::Decrypt(
    const std::uint8_t*        ciphertext,
    std::size_t                ciphertextSize,
    std::vector<std::uint8_t>& output)
{
    if (state_ != HandshakeState::Established)
        return SecureChannelResult::InternalError;

    if (ciphertextSize < 8 + kPoly1305TagBytes)
        return SecureChannelResult::BufferTooSmall;

    // Parse the inline sequence number and use it to rebuild the
    // nonce.  The replay-window enforcement lives outside this
    // class (`ReplayGuard`), but we still refuse an obviously stale
    // sequence that would wrap below the last acknowledged one.
    std::uint64_t sequence = 0;
    for (int i = 0; i < 8; ++i)
        sequence |= static_cast<std::uint64_t>(ciphertext[i]) << (i * 8);

    std::uint8_t nonce[kChaChaNonceBytes];
    BuildNonce(sequence, nonce);

    const std::size_t payloadSize = ciphertextSize - 8 - kPoly1305TagBytes;
    output.resize(payloadSize);

    const bool opened = ChaCha20Poly1305Open(
        config_.sessionKey.data(),
        nonce,
        /*aad*/ ciphertext,
        /*aadSize*/ 8,
        /*ciphertext*/ ciphertext + 8,
        /*ciphertextSize*/ payloadSize,
        /*tag*/ ciphertext + 8 + payloadSize,
        /*out*/ output.data());

    if (!opened)
    {
        // Zero on failure — the caller must not see any partial
        // plaintext on an authentication mismatch.
        std::fill(output.begin(), output.end(), std::uint8_t{0});
        output.clear();
        return SecureChannelResult::AuthenticationFailed;
    }

    rxSequence_ = sequence;
    return SecureChannelResult::Ok;
}

} // namespace SagaEngine::Networking
