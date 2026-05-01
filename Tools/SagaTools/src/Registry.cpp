/// @file Registry.cpp
/// @brief Two-section registry parser — `{tools:{...},installers:{...}}` JSON.

#include "SagaTools/Registry.h"
#include "SagaTools/Encoding.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace SagaTools
{

namespace
{

// ─── Tiny JSON Parser ────────────────────────────────────────────────────────

class JsonParser
{
public:
    explicit JsonParser(std::string text) : mText(std::move(text)) {}

    void Parse(std::vector<Registry::Entry>& outTools,
               std::vector<Registry::Entry>& outInstallers)
    {
        SkipWs();
        Expect('{');
        bool first = true;
        while (true)
        {
            SkipWs();
            if (Peek() == '}') { Consume(); return; }
            if (!first) { Expect(','); SkipWs(); }
            first = false;

            const std::string key = ReadString();
            SkipWs();
            Expect(':');
            SkipWs();

            if      (key == "tools")      ParseFlatObject(outTools);
            else if (key == "installers") ParseFlatObject(outInstallers);
            else                          SkipValue();
        }
    }

private:
    void ParseFlatObject(std::vector<Registry::Entry>& out)
    {
        SkipWs();
        Expect('{');
        bool first = true;
        while (true)
        {
            SkipWs();
            if (Peek() == '}') { Consume(); return; }
            if (!first) { Expect(','); SkipWs(); }
            first = false;

            const std::string key = ReadString();
            SkipWs();
            Expect(':');
            SkipWs();
            const std::string value = ReadString();
            out.emplace_back(key, value);
        }
    }

    void SkipWs()
    {
        while (mPos < mText.size())
        {
            const char c = mText[mPos];
            if (std::isspace(static_cast<unsigned char>(c))) { ++mPos; continue; }
            if (c == '/' && mPos + 1 < mText.size() && mText[mPos + 1] == '/')
            {
                while (mPos < mText.size() && mText[mPos] != '\n') ++mPos;
                continue;
            }
            return;
        }
    }

    char Peek()    { if (mPos >= mText.size()) Fail("unexpected end of input"); return mText[mPos]; }
    char Consume() { if (mPos >= mText.size()) Fail("unexpected end of input"); return mText[mPos++]; }

    void Expect(char c)
    {
        SkipWs();
        if (mPos >= mText.size() || mText[mPos] != c)
        {
            std::ostringstream msg;
            msg << "expected '" << c << "' at offset " << mPos;
            Fail(msg.str());
        }
        ++mPos;
    }

    /// Read 4 hex digits and return the integer value, or throw on bad input.
    unsigned ReadHex4()
    {
        if (mPos + 4 > mText.size()) Fail("incomplete \\uXXXX escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char h = mText[mPos++];
            v <<= 4;
            if      (h >= '0' && h <= '9') v |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') v |= 10u + static_cast<unsigned>(h - 'a');
            else if (h >= 'A' && h <= 'F') v |= 10u + static_cast<unsigned>(h - 'A');
            else Fail("invalid hex digit in \\uXXXX escape");
        }
        return v;
    }

    /// Append a Unicode codepoint as UTF-8 bytes. Used by \uXXXX decoding so
    /// paths like "C:\\Users\\...Masaüstü\\..." round-trip correctly even when
    /// the producer used ASCII-safe JSON escaping.
    static void AppendUtf8(std::string& out, unsigned cp)
    {
        if (cp < 0x80u) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }

    std::string ReadString()
    {
        SkipWs();
        Expect('"');
        std::string out;
        while (mPos < mText.size())
        {
            char c = mText[mPos++];
            if (c == '"') return out;
            if (c == '\\' && mPos < mText.size())
            {
                char e = mText[mPos++];
                switch (e)
                {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'n':  out.push_back('\n'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'u':
                    {
                        unsigned cp = ReadHex4();
                        if (cp >= 0xD800u && cp <= 0xDBFFu)
                        {
                            // High surrogate — pair with the next \uXXXX.
                            if (mPos + 2 <= mText.size()
                                && mText[mPos] == '\\' && mText[mPos + 1] == 'u')
                            {
                                mPos += 2;
                                const unsigned lo = ReadHex4();
                                if (lo >= 0xDC00u && lo <= 0xDFFFu)
                                {
                                    cp = 0x10000u
                                       + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                                }
                            }
                        }
                        AppendUtf8(out, cp);
                        break;
                    }
                    default:   out.push_back(e);    break;
                }
                continue;
            }
            out.push_back(c);
        }
        Fail("unterminated string");
        return {};
    }

    void SkipValue()
    {
        SkipWs();
        const char c = Peek();
        if (c == '"') { ReadString(); return; }
        if (c == '{') { Consume(); int d = 1; while (d && mPos < mText.size()) { char x = mText[mPos++]; if (x == '"') { --mPos; ReadString(); } else if (x == '{') ++d; else if (x == '}') --d; } return; }
        if (c == '[') { Consume(); int d = 1; while (d && mPos < mText.size()) { char x = mText[mPos++]; if (x == '"') { --mPos; ReadString(); } else if (x == '[') ++d; else if (x == ']') --d; } return; }
        while (mPos < mText.size() && mText[mPos] != ',' && mText[mPos] != '}' && mText[mPos] != ']'
               && !std::isspace(static_cast<unsigned char>(mText[mPos])))
            ++mPos;
    }

    [[noreturn]] void Fail(const std::string& reason) const
    {
        throw std::runtime_error("tools.registry.json parse error: " + reason);
    }

    std::string mText;
    std::size_t mPos = 0;
};

// ─── Path helpers (UTF-8 aware) ──────────────────────────────────────────────

bool FileExists(const std::string& utf8Path)
{
    std::error_code ec;
    return std::filesystem::exists(PathFromUtf8(utf8Path), ec);
}

std::string DirectoryOf(const std::string& filePath)
{
    namespace fs = std::filesystem;
    const fs::path parent = PathFromUtf8(filePath).parent_path();
    return parent.empty() ? std::string(".") : PathToUtf8(parent);
}

} // namespace

// ─── Registry-file Discovery ──────────────────────────────────────────────────

std::string Registry::LocateFile(const std::string& explicitPath,
                                 const std::string& executableDir) noexcept
{
    namespace fs = std::filesystem;

    if (!explicitPath.empty() && FileExists(explicitPath))
        return PathToUtf8(fs::absolute(PathFromUtf8(explicitPath)));

    const std::string envReg = GetEnvUtf8("SAGATOOLS_REGISTRY");
    if (!envReg.empty() && FileExists(envReg))
        return PathToUtf8(fs::absolute(PathFromUtf8(envReg)));

    if (!executableDir.empty())
    {
        const std::string sibling = executableDir + "/config/tools.registry.json";
        if (FileExists(sibling))
            return PathToUtf8(fs::absolute(PathFromUtf8(sibling)));
    }

    const std::string cwdCandidate = "tools.registry.json";
    if (FileExists(cwdCandidate))
        return PathToUtf8(fs::absolute(PathFromUtf8(cwdCandidate)));

    return {};
}

// ─── Loading ─────────────────────────────────────────────────────────────────

std::optional<Registry> Registry::LoadFromFile(const std::string& filePath,
                                               std::string*       outError)
{
    // Open via the UTF-8-aware path so non-ASCII characters in the path
    // do not get mangled by Windows' ANSI code page.
    std::ifstream in(PathFromUtf8(filePath));
    if (!in)
    {
        if (outError) *outError = "cannot open registry file: " + filePath;
        return std::nullopt;
    }

    std::ostringstream buf;
    buf << in.rdbuf();

    Registry registry;
    registry.mBaseDir = DirectoryOf(filePath);

    try
    {
        JsonParser parser(buf.str());
        parser.Parse(registry.mTools, registry.mInstallers);
    }
    catch (const std::exception& ex)
    {
        if (outError) *outError = ex.what();
        return std::nullopt;
    }

    return registry;
}

// ─── Lookup ──────────────────────────────────────────────────────────────────

const std::string* Registry::FindToolRaw(const std::string& name) const noexcept
{
    for (const auto& e : mTools)
        if (e.first == name) return &e.second;
    return nullptr;
}

const std::string* Registry::FindInstallerRaw(const std::string& name) const noexcept
{
    for (const auto& e : mInstallers)
        if (e.first == name) return &e.second;
    return nullptr;
}

} // namespace SagaTools
