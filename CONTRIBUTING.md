# Contributing To SagaEngine

Contributions must preserve SagaEngine's ownership, architecture, evidence, and
source-provenance boundaries.

## Third-Party Source And Provenance Policy

### Acceptable Sources

SagaEngine contributions may contain:

- code written independently by the contributor;
- code used under a compatible, verified open-source license;
- public-domain code with verifiable provenance;
- clean-room implementations based on public specifications, documentation, or
  observable behavior;
- generated code whose generator, inputs, ownership, and applicable license are
  recorded.

### Prohibited Sources

Do not contribute code, data, tests, constants, comments, layouts, or generated
output derived from:

- leaked or stolen source code;
- proprietary source code without explicit authorization;
- code with unknown or unverifiable licensing;
- renamed, translated, refactored, decompiled, or mechanically transformed
  copies of prohibited source;
- AI output produced from prohibited or tainted source material.

Changing names, languages, formatting, or control flow does not make
unauthorized source acceptable.

### AI-Assisted Contributions

AI-assisted code is held to the same provenance, licensing, security, review,
and test standards as manually written code.

The contributor remains responsible for:

- reviewing the complete generated diff;
- confirming that no confidential, leaked, proprietary, or license-unclear
  material was used as input;
- checking suspiciously specific algorithms, constants, comments, layouts, and
  identifiers;
- satisfying all third-party notices and attribution obligations;
- running the relevant build and tests;
- removing generated code whose origin or rights cannot be explained.

An AI tool output is not evidence of independent authorship, license
compatibility, correctness, or security. Do not send repository secrets,
private keys, restricted customer data, or unauthorized third-party source to
external AI services.

### Required Provenance Record

For imported, adapted, or externally derived code, record:

- upstream project and repository;
- exact source revision or release;
- source file or component;
- selected license and required notices;
- copyright owner where available;
- local modifications;
- transitive third-party dependencies;
- SagaEngine owner and consumers;
- update, replacement, and removal path.

Prefer importing the original upstream dependency over copying an
engine-specific wrapper when that produces a smaller and clearer dependency
boundary.

### Review And Escalation

When provenance or license compatibility is unclear:

- do not merge the contribution;
- isolate the proposed material from Saga-owned code;
- ask the maintainer responsible for licensing or governance;
- obtain qualified legal review before commercial or public distribution when
  the risk is material.

This policy is an engineering contribution rule and is not legal advice. The
repository's license, notice, contributor, and third-party files remain legally
authoritative.

<!-- SAGA-LICENSING-DCO:BEGIN -->
## Licensing, DCO, and contribution authority

SagaEngine uses Developer Certificate of Origin 1.1. Every contribution must
carry a valid `Signed-off-by` trailer from a person with authority to submit
the work.

Contributions use inbound-equals-outbound: the contribution is accepted under
the current effective license of the affected file or path at merge time. A
future target recorded in `LICENSE_POLICY.toml` is not the inbound license
until its migration becomes effective.

Contributors must disclose copied, adapted, generated, or AI-assisted material
and retain sufficient provenance evidence. A contribution must not import
material under incompatible terms or material the contributor lacks authority
to submit.

Squash, co-authored, bot-assisted, and generated commits must preserve valid
DCO and provenance records. Contributions to unresolved licensing domains may
be frozen until the domain is classified.
<!-- SAGA-LICENSING-DCO:END -->
