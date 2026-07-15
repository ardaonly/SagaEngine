---
title: Licensing authority
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Licensing authority

The repository license policy is MPL-centered for engine/editor source. Forge, imported content, and third-party/vendor areas follow their own recorded license domains and notices.

## Authorities

- `LICENSE` is the primary repository license text.
- `LICENSES/THIRD_PARTY_NOTICES.md` is the authoritative third-party notice.
- additional SPDX texts live under `LICENSES/`;
- `LICENSE_POLICY.toml` defines policy domains and path ownership;
- `LICENSE_TEXT_SOURCES.toml` and `.reuse/dep5` support validation and reuse metadata.

Generated path rules, checksum files, and validation reports are derived control outputs. They help detect drift but do not outrank the policy and recorded license texts from which they were produced.

## Stability

Changing a header, moving a path, or regenerating a checksum must not silently change licensing intent. A new licensing domain requires an explicit policy decision, path scope, and notice treatment.

There is no stable public plugin domain or plugin contract today. Historical plugin licensing proposals are private reference material for a future decision, not current public policy.

## Contribution authority

Contributions use DCO 1.1 and require a valid `Signed-off-by` trailer from a person authorized to submit the work. Inbound-equals-outbound applies to the effective license of the affected path. Copied, adapted, generated, and AI-assisted material requires reviewable provenance and compatible rights.

`SagaWiki/policy/contributing-dco.md` is the maintained Markdown fragment for this contribution rule. The Wiki builder emits the policy-required `SagaWiki/pages/licensing.html` bridge from it. That bridge is not a browsable Wiki page or a second documentation source.

## Documentation rule

SagaWiki summarizes the authority chain; it does not duplicate every license term. When this page and policy files disagree, treat that as documentation drift and resolve it against the repository authorities.

See [Licensing, contribution, and third-party governance](../reference/licensing-contribution-and-third-party.md) for the complete domain, DCO, provenance, dependency acceptance, vendor, artifact-specific notice, and validation rules.
