---
title: Build, package, and distribution contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Build, package, and distribution contracts

Build, package, distribution validation, and publish are separate stages with separate owners and evidence. This reference retains the durable Linux layout, archive/checksum, smoke, tool-packaging, Nix, and publish-policy knowledge while using current 0.0.11 paths.

## Tool ownership

- Forge prepares dependency/profile state.
- CMake owns target declaration, dependency visibility, build, install, and export composition.
- SagaProjectKit validates `.sagaproj` and project reports.
- SagaScript owns script analysis/compile/projection/patch artifacts.
- SagaPackager validates package inputs, stages the Linux candidate, creates archive/checksum, and emits preflight/report data.
- `Tools/Developer/DistributionCheck` owns whitelist, unpacked smoke, candidate, contract, state, and chaos checks.
- repository scripts are stable routing wrappers, not alternate implementations.

## Development environment

The live Linux preset is `linux-gcc`. Forge prepares dependencies before CMake configure/build when the environment requires it:

```sh
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc"
```

Nix provides a reproducible development shell and tool discovery boundary. It does not prove reproducible binaries across machines, hermetic source fetches, or clean-machine runtime portability unless the complete inputs and resulting artifacts are controlled and compared.

Documentation uses the preset and discovers the active configured build directory. It should not bake historical `build/RelWithDebInfo-0.0.x` paths into durable architecture text.

## CMake target ownership

Each runtime/editor module owns its `CMakeLists.txt`. Programs own thin executable targets. Top-level CMake composes modules and shared policy. A target's public include paths, public dependencies, install/export role, and sources must match its semantic owner.

`PUBLIC` dependencies are justified by public headers; implementation-only vendors/backends are `PRIVATE`. Tests link their own helper/private targets. Aggregate targets retained through the cutover are transitional and do not authorize new cross-owner source lists.

Build success alone does not prove target purity. Architecture/installed-consumer checks catch source ownership, private include, vendor leakage, and export issues.

## Build outputs

Build directories, generated code/artifacts, object files, binaries, reports, package staging, and caches are disposable output. They are not input to source review, Wiki generation, or licensing classification except through explicit generated-output rules.

A stale binary can appear runnable after a source/build failure. Validation records the actual build command/result and removes or avoids stale package outputs when preflight fails.

## Package input

SagaPackager receives an explicit project/profile and resolved build/tool/sample/license inputs. It does not search arbitrary legacy roots. Required input records distinguish available, missing, invalid, and incompatible.

Optional governance/policy reports are externally produced precomputed inputs. SagaPackager can accept and validate them for a publish decision; it does not generate them and there is no bundled SagaPolicyKit workflow.

Package policy validates project/source/artifact alignment, package profiles, required binaries, executable bits, documentation/license material, sample/tool inputs, and output destinations. Missing input blocks package success rather than producing a plausible incomplete archive.

## Staged Linux layout

The staged candidate has one controlled `Saga/` root and an exact whitelist. Current policy can stage items such as:

- `bin/` program/tool launchers and selected binaries;
- published application files needed by `sagaproject`, `sagascript`, and `sagapack`;
- selected sample/project material required by the declared workflow;
- current documentation/readme/verification/known-limitation files;
- `VERSION` and package metadata;
- `licenses/` with Saga license and applicable third-party notices.

Directories such as `lib/`, `include/`, or `share/saga/` appear only when real selected artifacts exist. An empty placeholder is not staged to imply a public SDK, native library distribution, templates, profiles, schemas, plugins, or CMake package.

Tests, source trees, build caches, local audit folders, private state, and arbitrary unapproved files are forbidden from the candidate. The exact whitelist rejects unknown additions as well as missing required files.

## Packaged tools

Distribution launchers for `sagaproject`, `sagascript`, and `sagapack` target their staged published application files. They are different from repository development wrappers. Tests invoke the candidate launchers from the unpacked tree and do not accidentally fall back to the source checkout.

The current tools are framework-dependent .NET applications. A compatible runtime remains an external requirement unless a future package changes the publication model. Staging the application files does not claim a self-contained runtime.

Wrapper generation fixes relative-path resolution against the package root, quotes arguments, preserves exit codes, and fails clearly when the staged application/runtime is missing. It must not call back into the source repository.

## Preflight report

Preflight is a structured gate over current inputs. It records required, available, and missing entries by category; resolved paths; selected profile/project; layout status; archive/checksum status; blockers; limitations; and final status.

`preflight-passed` means the currently declared checks passed. It is not maintainer verification, release approval, clean-machine portability, security review, or product readiness.

On a blocking failure, stale staged layout/archive/checksum output is removed or clearly invalidated. A consumer must not find yesterday's `Saga.tar.zst` beside today's failed report and mistake it for new output.

## Archive contract

The Linux archive is `Saga.tar.zst` containing one `Saga/` root. Generation requires a `tar` path with Zstandard support and `zstd` according to current policy. Archive entry validation rejects absolute paths, traversal, entries outside the root, test payload directories, forbidden source surfaces, unknown directories/files, and forbidden executable names.

Archive creation is deterministic where inputs/tool support permit: stable root, controlled file set, normalized metadata policy as implemented, and no ambient working-tree files. The current tool/report states any reproducibility limitations rather than promising byte-identical output without proof.

An archive listing check proves shape and entry safety, not that every executable runs.

## Checksum contract

`Saga.sha256` records the SHA-256 digest and archive filename. Verification runs from the directory containing the archive/checksum so filename resolution cannot accidentally use another file. A mismatch blocks unpack/smoke.

A valid checksum proves integrity of the referenced archive bytes relative to the recorded digest. It does not prove authenticity, code signing, malware safety, license completeness, runtime correctness, release approval, or secure delivery.

## Unpacked smoke

The smoke verifies checksum, archive root/whitelist, unpack, unpacked exact layout, executable bits, and bounded tool/sample workflows. It runs from a temporary unpack root and avoids source-tree dependencies.

Headless smoke can invoke selected `--help`/report commands and a named sample workflow. SagaEditor can be presence-checked without launching GUI in a headless environment. A missing display is not converted into proof that the editor launches.

Every command records executable path, arguments, working directory, exit state, stdout/stderr or report path, and limitations. A skip due to a failed earlier checksum/unpack remains skipped/blocked, not passed.

## Distribution contract verification

The contract verifier joins package report, smoke report, layout, archive, checksum, version/license/docs, and exact whitelist evidence. It checks that the reports correspond to the same candidate and that required stage statuses are acceptable.

This aggregation does not broaden claims beyond its inputs. It is valuable because it prevents a successful checksum from hiding a failed tool smoke or a passing package report from referring to another output directory.

## Native dependency closure

The current Python packaging path does not stage complete native shared-library dependency closure or prove clean-machine portability. A real distribution review must inspect each executable/library's dynamic dependencies, runtime search paths, system assumptions, graphics/window dependencies, Qt plugins, .NET runtime requirement, and platform licenses.

Passing on the development machine can rely on Nix/Conan/system libraries absent elsewhere. Clean-machine/container/VM testing is separate evidence.

## Publish boundary

Publishing consumes a known package/archive plus evidence and an explicit approval. It may include dry run, destination/channel, immutable artifact hash, policy inputs, and resulting publication record. It must not rebuild silently between approval and upload; hashes tie approval to bytes.

The current repository contains publish/readiness contracts and local gate values. That does not establish a hosted distribution service or actual upload workflow. “Ready” and “published” remain distinct statuses.

Dangerous-operation and role/policy reports can block or require review, but local metadata is not secure authentication. The packager/publisher still validates paths, hashes, destination, and credentials through its owner.

## Artifact-specific licenses

Before an archive is distributable, determine which third-party components are actually present, how they are linked, and which notices/texts/recipient rights apply. Repository-wide dependency inventory is not copied blindly into every artifact. The artifact composition is the notice authority for that release record.

Qt shared distribution, Diligent/vendor material, SDL/RmlUi, persistence libraries, OpenSSL, .NET/runtime, and transitive components are included in the review only when the candidate actually ships them or creates an applicable obligation.

## Failure classes

Package/distribution failures distinguish invalid project/profile, missing binary/tool/sample/doc/license, unapproved extra file, wrong executable bit, stale input/artifact, archive-tool unavailable, archive generation/list/root failure, checksum write/mismatch, unpack failure, tool smoke failure, report mismatch, policy denial, and unsupported environment.

Reports retain the first/root blockers and subsequent skipped checks. They do not collapse everything into “not ready” without actionable cause.

## Evidence and non-claims

Layout validation proves selected files and absence of forbidden files. Archive validation proves safe shape. Checksum proves byte integrity. Unpacked smoke proves named commands/workflows in that environment. Contract aggregation proves those evidence records agree.

None alone or together currently proves a final release, public SDK distribution, full native dependency closure, clean-machine portability, production editor/runtime/server readiness, code signing, secure updater, installer, multi-platform distribution, or maintainer approval.

## Change checklist

- Keep Forge, CMake, project/script/package tools, and developer checks as distinct owners.
- Use current presets/paths and avoid historical build-directory literals.
- Stage from explicit validated inputs into an exact whitelist.
- Remove/invalidate stale output on failure.
- Keep development wrappers separate from distribution launchers.
- Verify one safe archive root and traversal-free entries.
- State exactly what checksum and smoke prove.
- Review actual native/runtime dependency closure and artifact-specific notices.
- Tie approval/publish to immutable hashes and retain explicit non-claims.
