---
title: Licensing, contribution, and third-party governance
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Licensing, contribution, and third-party governance

This reference explains the current authority chain and durable review rules. It does not replace the legal texts or `LICENSE_POLICY.toml`. If the Wiki disagrees with those authorities, treat the Wiki as drift and resolve the discrepancy before distributing or accepting affected work.

## Current direction

Saga-owned engine, runtime, editor, programs, tests, CMake, scripts, profiles, and other classified domains follow the active MPL-centered policy recorded in `LICENSE_POLICY.toml`. Editor source is not currently governed by the historical read-only/no-AI proposal. Forge, vendor/imported content, and other specifically classified areas follow their own recorded domains and upstream terms.

There is no stable public plugin domain or plugin API contract. Historical plugin licensing material remains private/archive reference for Arda if a real plugin contract is created. It is not current public policy.

## Authority order

The important authorities are:

1. applicable legal grants and verified upstream terms;
2. effective migration/decision records and the primary `LICENSE` text;
3. embedded SPDX metadata, sidecars, and `.reuse/dep5` as applicable;
4. active `LICENSE_POLICY.toml` domain/path policy;
5. `LICENSES/THIRD_PARTY_NOTICES.md` and included SPDX/upstream texts;
6. generated path rules, checksums, reports, and Wiki summaries as derived controls.

`LICENSE_TEXT_SOURCES.toml` records text-source provenance. `SHA256SUMS` and generated path rules detect drift; they do not grant rights or supersede a license.

## Domain model

A licensing domain has id, priority, path scope, owner, rights-holder reference, current/target expression, resolution rule, provenance/authority review, and migration status/effective commit where applicable.

Path overlap is deliberate and resolved by policy priority/specificity rules. An unclassified file is reported or denied according to validation mode; it is not silently assumed to share a neighboring directory's license. A move across domains can change obligations and requires review even when file content is unchanged.

Required path rules must match real current roots. Orphan rules for retired roots are policy debt and should be removed through a policy change, not satisfied by recreating an empty legacy directory.

## MPL-centered meaning

MPL-2.0 applies at file level according to its terms. It is not equivalent to a permissive license and is not a blanket relicensing of third-party code. Modifications to MPL-covered files and distribution of covered source follow the license obligations; separate files can remain under compatible terms according to composition.

This Wiki does not provide legal advice. Release review uses the license text and actual artifact composition.

## Stability rule

A rename, generated checksum update, or header edit must not silently change licensing intent. Changing a domain license, adding a restricted/imported/vendor domain, or altering redistribution strategy requires an explicit decision, path scope, rights/provenance basis, effective point, and notice/update plan.

Future target fields in policy do not become effective inbound/outbound terms until the recorded migration becomes effective. Contributors use the current effective license of the affected path at merge time.

## DCO

SagaEngine uses Developer Certificate of Origin 1.1. Each contribution carries a valid:

```text
Signed-off-by: Name <email>
```

The sign-off is the contributor's certification that they have the right to submit the work under the project's effective terms. It is not decorative metadata. Squash, co-authored, bot-assisted, and generated commits must preserve valid authority and provenance records.

Inbound-equals-outbound means the contribution is accepted under the current effective license of the affected file/path. Work touching multiple domains must be compatible with each domain.

`SagaWiki/policy/contributing-dco.md` is the maintained Markdown fragment for the contribution rule. `scripts/wiki build` generates the policy-required `SagaWiki/pages/licensing.html` bridge. The bridge exists for machine policy compatibility; it is not a second source or normal Wiki article.

## Copied, generated, and AI-assisted material

Contributors disclose copied, adapted, generated, or AI-assisted material and retain enough provenance to review rights and compatibility. “Generated” does not mean ownerless or automatically safe. Prompts, source references, model/tool terms, human authorship/review, and copied fragments can matter.

Do not import material with unknown origin, incompatible license, non-commercial/field-of-use restriction, or terms the contributor lacks authority to accept. When provenance cannot be resolved, keep the contribution out of the affected domain.

Generated output follows the license of its template/input/tool contract only where that policy is actually established. Minimal scaffolding may use an approved default, but there is no automatic exception for every file emitted by a tool.

## Third-party boundary

Third-party software remains under upstream terms. SagaEngine's repository license does not relicense SDL, RmlUi, GLM, nlohmann/json, Diligent and its third-party graph, Qt, OpenSSL, Asio, libpqxx/libpq, hiredis, redis-plus-plus, Dear ImGui, GoogleTest, RapidCheck, CMake, Ninja, or other dependencies.

The authoritative notice records reviewed versions/configuration and integration role. Conan/build metadata is inventory evidence; upstream license files and project terms remain authoritative.

## Dependency acceptance

Default review posture:

| Terms | Default posture |
| --- | --- |
| MIT/BSD/Zlib/BSL and comparable permissive | Allow after notice and redistribution verification |
| Apache-2.0 | Allow after NOTICE and patent-term verification |
| MPL-2.0 | Review composition/file obligations |
| LGPL | Review linking, recipient rights, replacement/relinking, and distribution configuration |
| GPL/AGPL | Explicit review |
| Unknown/custom | Deny until reviewed |
| Non-commercial or field-of-use restriction | Deny for normal open-source product path |
| Binary-only/NDA/platform SDK | Separate restricted review |

A permissive copyright license does not resolve patents, codec royalties, export controls, registrations, trademarks, platform terms, or binary redistribution restrictions.

## Vendor and imported content

`Vendor/Diligent` is a pinned upstream submodule graph and retains upstream terms. Its embedded third-party directories retain their own notices. Saga-owned wrapper/adapter files follow the Saga domain only where provenance and path policy say so.

Imported/sample/content assets can carry creator, source URL, license, modification, and attribution requirements different from engine code. A source asset and a cooked derivative both need traceable rights. Do not assume an output loses the input license.

Forge and package/dependency metadata are classified according to current policy. Tooling that downloads dependencies does not transfer rights to redistribute them.

## Qt and dynamic distribution

The reviewed editor direction uses shared Qt. A distribution containing Qt must preserve applicable license texts/notices and recipient rights. Static Qt distribution is outside the reviewed default and requires separate analysis.

Actual Qt plugins and libraries shipped determine obligations. Merely depending on Qt in source does not mean every artifact contains Qt; conversely, a packaged editor must not omit applicable material because a generic engine-only notice was used.

## Persistence and server dependencies

libpqxx/libpq, hiredis, and redis-plus-plus apply only to artifacts that actually link/package the corresponding implementation. Public vendor-neutral persistence interfaces do not themselves require shipping those binaries. The package/release record inspects the real link/runtime closure.

## Artifact-specific release review

For each binary, SDK, archive, installer, container, or package:

1. inventory components actually included or statically/header-only incorporated;
2. record dynamic/system-provided dependencies separately;
3. identify each applicable license text, copyright, attribution, NOTICE, source/relink/replacement right;
4. inspect vendor/transitive components and plugins actually present;
5. check patent/codec/platform/export/registration concerns;
6. assemble the notice bundle for that artifact;
7. retain a release record tying inventory/notices to artifact hash.

A repository-wide dependency list must not be copied blindly into every package. An engine graphics SDK, editor archive, server binary, test bundle, and source release can have different compositions.

## Generated policy controls

`Tools/Licensing/validate_license_policy.py` validates schema, paths, overlaps, metadata, governance, texts, and other policy checks. The generated legacy `path_rules.json` is compatibility output sourced from `LICENSE_POLICY.toml`. Checksum regeneration records current controlled file hashes.

Validation reports are evidence. A zero-error report means configured checks passed at that tree state; it does not resolve unknown upstream legal facts or approve a release automatically.

## Documentation and continuity

SagaWiki explains policy in human-readable form. Historical license proposals that are no longer effective remain in Git/private continuity, not mixed into current wording. License stability protects current rights by requiring explicit changes rather than carrying every historical debate into public docs.

## Failure and escalation

Unknown license, conflicting metadata, unresolved provenance, domain overlap, external symlink/submodule ambiguity, LFS pointer where forbidden, missing governance text, or artifact notice gap blocks or escalates according to policy. Do not “fix” a validation error by deleting attribution or changing SPDX without authority.

## Change checklist

- Identify the current licensing domain before editing/moving a file.
- Preserve SPDX and provenance unless an authorized decision changes them.
- Require DCO sign-off and disclose generated/copied/AI-assisted sources.
- Keep third-party terms and notices intact.
- Review actual artifact composition, linking, plugins, and transitive closure.
- Treat generated rules/checksums/reports as controls, not grants.
- Keep plugin licensing historical/private until a real plugin contract exists.
- Escalate unknown or conflicting rights instead of guessing.
