---
title: C# and Visual Blocks contracts
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# C# and Visual Blocks contracts

C# source is the behavior source of truth. Visual Blocks provide an editor projection and bounded authoring surface over compatible source. Generated descriptors, block graphs, IR, projection reports, compiled assemblies, and patch reports are derived artifacts; none is an independent runtime behavior authority.

## One behavior truth

The durable direction is:

```text
C# source
  -> analyze and classify
  -> project supported constructs with source spans
  -> preview a supported semantic edit
  -> apply an exact freshness-checked source patch
  -> analyze and compile the resulting C#
  -> execute compiled C# through the scripting runtime
```

There is no second graph VM in this contract. Runtime does not interpret the editor graph as an alternative implementation of the script. A block is useful because it maps to C# syntax/semantics and can preserve unsupported source, not because it replaces the language.

## Ownership

The scripting tool/runtime owner analyzes, compiles, and applies source patches. `VisualBlocksEditor` publicly owns only the evidenced `Blocks/**` authoring values and lowering contract. `EditorAuthoring` can display scripting-tool reports and request reviewed changes; it does not directly overwrite C#.

Compiler, debugger, editor, graph/document, import, node, pin, type-system, evaluation, and runtime-host placeholder APIs are not part of the Visual Blocks public surface. The private product integration can derive a read-only descriptor for evidence, but that descriptor is not behavior authority or a stable editor SDK.

## Compatibility profile

The compatibility profile classifies source without mutating it. Useful categories are:

| Category | Meaning |
| --- | --- |
| Projectable | The construct can be represented faithfully with a source span and supported block metadata. |
| Partially projectable | Supported constructs can be shown, while advanced regions remain preserved and read-only. |
| Advanced opaque | Valid C# remains source text with a span/diagnostic and is not safely editable as blocks. |
| Unsupported/invalid | The construct or file lies outside the accepted profile or cannot be parsed/validated. |

Classification never changes whether valid C# is canonical. “Opaque” means the visual authoring layer must preserve and avoid editing that region. “Unsupported” is explicit evidence, not permission to delete or regenerate the code.

## Authoring levels

High-level blocks express intention-oriented gameplay behavior through supported calls and simple parameters. Low-level blocks expose more explicit runtime/authority/diagnostic contracts only when their semantics and safety can be represented. Unsafe/advanced C# remains text-only or rejected from the block surface.

The levels are authoring affordances, not different runtimes. High-level and low-level operations ultimately produce C# and use the same compiler/runtime. Unsafe access requires explicit source and review; it is never smuggled into a friendly block label.

Advanced/opaque examples include reflection-heavy behavior, complex generics, arbitrary async/threading, unsupported I/O, unsafe code, dynamic behavior, lambdas/LINQ outside the profile, preprocessor-sensitive rewrites, and control flow the patch engine cannot preserve. The exact supported set follows current analyzer evidence.

## Projection artifact

A projection artifact is read-only evidence generated for a specific source version. Each projected record includes:

- stable block/construct id within the artifact contract;
- block kind and display metadata;
- source file relative to the approved root;
- exact byte span or explicitly non-editable/no-span status;
- source hash/fingerprint;
- compatibility/editability classification;
- diagnostics and opaque/unsupported preservation data;
- artifact schema and tool/command identity.

Possible kinds include script/class, callable method, parameter, return, attribute, literal/call, opaque source region, and unsupported diagnostic blocks. A kind name in a descriptor does not mean edits for that kind are implemented.

Projection does not normalize formatting, reorder members/usings, or emit replacement C#. It reads source and reports a view. Source bytes remain unchanged, including comments and opaque regions.

## Blocks and IR

Block stacks and block IR represent bounded authoring structure for editor analysis and lowering. They are intermediate/editor values. If they are serialized, the file is still derived unless a future project schema deliberately establishes a canonical visual-authoring format.

Lowering validates block kind, slot compatibility, required inputs, supported operation, and deterministic ordering. Missing inputs, type mismatch, invalid snapping, or unsupported blocks produce diagnostics.

There is no public graph evaluator or Visual Blocks runtime host. Runtime behavior comes from compiled C#.

## Gameplay node library

Node classification communicates abstraction and evidence:

- high-level gameplay nodes describe bounded intention such as invoking a supported gameplay action with typed parameters;
- low-level nodes expose explicit engine operations for advanced authors where a stable contract exists;
- unsafe/unsupported operations stay out of friendly authoring surfaces or require explicit textual code.

Each node descriptor should state operation id, category/level, inputs/outputs, type/range/defaults, authority/thread requirements, side effects, deterministic behavior, source mapping, and current evidence. A node does not call an arbitrary implementation by string reflection without validation.

Server-authoritative actions remain requests from client/editor-authored behavior. A node cannot grant authority or bypass validation.

## Operation request

A block edit request is separate from projection. It identifies operation id/kind, target block id, requested typed value, and the projection/source version the user observed. Known operation vocabulary can be broader than implemented apply behavior; unsupported known kinds return a clear rejection.

The first safely evidenced form is a minimal literal edit, especially a string literal. Other possible bounded literal or method-argument edits require their own parser/escape/type/range fixtures before being accepted. Whole-method/class/file regeneration is outside the policy.

Rejected operations include arbitrary source rewrite, method-body regeneration, class restructure, using/formatting rewrite, opaque/unsupported region edit, advanced C# lowering, async/reflection/unsafe rewrite, and control-flow restructure.

## Patch preview

Preview is non-mutating. The report includes schema/tool/command, source file, operation id/kind, target block, target source hash/span, requested value, status, minimal replacement description, diagnostics, source-preservation statement, and `mutatesSource: false`.

A passing preview proves only that the current projection and operation can describe one exact replacement. It does not reserve the file or guarantee later application. Apply rechecks every relevant invariant.

The replacement text is constructed according to C# lexical rules. A string value is escaped as a string literal; a caller cannot inject unquoted source through a “string value” field. Numeric/boolean operations validate type and supported spelling policy.

## Apply policy

Source-changing behavior is owned by the scripting/source tool. Apply must:

1. Resolve the target beneath the approved source root.
2. Re-read the file and compute its current hash.
3. Reject if the expected and actual hashes differ.
4. Validate the byte span against current file bounds and construct boundaries.
5. Confirm the operation kind and replacement remain permitted.
6. Create the candidate by replacing only that span.
7. Verify every byte outside the approved span is unchanged.
8. Write a copied output by default, or use an explicit approved in-place transaction.
9. For in-place work, use temporary-file, fsync/close as appropriate, atomic rename, backup, and deterministic rollback policy.
10. Analyze/compile or run the declared check against the candidate.
11. Emit a versioned apply report with original/patched hash, paths, status, diagnostics, and preservation result.

The default safe path writes `patched-source/<relative-source-path>` plus an apply report under an output directory. It does not overwrite the original file.

## Staleness

Hash and span are both required. Span alone can target the wrong text after an earlier insertion. Hash alone does not identify the approved token. Operation/block identity ties the human-reviewed action to both.

On staleness, apply stops. It does not search for a similar literal elsewhere, re-run projection invisibly, or select the first text match. The user/tool generates a new projection and preview against current source.

Multi-file operations would require an atomic transaction protocol, per-file hashes/spans, dependency order, rollback, and review representation. They are not inferred from one-file patch support.

## Source preservation

Forbidden behavior includes whole-file rewrite, formatter normalization, using/member reorder, comment deletion, newline/encoding conversion outside the span, class/method regeneration, opaque-region mutation, and editor-side direct writes.

Preserving “semantics” is insufficient for this contract. Bytes outside the approved span remain identical. This protects manual formatting, comments, advanced code, and concurrent review assumptions.

Encoding and byte offsets must agree. If the artifact defines UTF-8 byte spans, the implementation does not reinterpret them as character indices. Invalid UTF-8/unsupported encoding is explicit failure.

## Compile and runtime boundary

After patching, analysis and compile evidence confirm the resulting C# is accepted by the current toolchain. A compile failure makes the workflow failed and preserves the candidate/report for inspection according to policy; it does not silently publish broken artifacts.

Compiled assemblies/manifests carry source and tool fingerprints. Runtime validates package/artifact freshness before loading. Any future reload path remains a scripting-runtime concern and cannot execute a Visual Blocks graph.

Managed/native bridges expose registered, typed, bounded calls. Script code cannot use the block projection to bypass binding, authority, thread, or permission rules. Bridge implementation is not a public Visual Blocks contract.

## Two-way authoring evidence

The bounded loop is:

```text
compatible C# source
  -> compatibility report
  -> read-only projection
  -> one supported edit preview
  -> copied patched source
  -> analyze and compile patched source
```

This proves source-preserving tool orchestration for the fixture/operation. It does not prove a visual editor UI, arbitrary C# round-trip, in-place multi-file editing, a complete node library, or runtime graph execution.

## Editor integration

Editor views consume artifacts and reports. A projection view shows projectable and opaque regions. A patch evaluation view shows exact target, old/new representation, freshness, diagnostics, and non-claims. A review workflow lets the user accept an explicit apply request.

The editor retains personal graph layout/selection separately from C# source. Moving a block for readability does not alter behavior unless an explicit supported source operation is generated and approved.

If source changes externally, views become stale and refresh. They never keep presenting old projection as current without a visible stale state.

## Diagnostics

Stable failure classes include missing source, path outside root, hash mismatch, invalid span, missing target block, non-editable target, opaque/unsupported region, unsupported operation, malformed replacement, preservation mismatch, output write failure, rollback failure, analyze failure, compile failure, and stale derived artifact.

Reports include source-relative paths and hashes without dumping full source by default. Diagnostics identify which invariant failed and whether any source was written.

## Non-claims

The current contract does not establish a finished Visual Blocks editor, arbitrary C# conversion, general AST rewrite engine, complete gameplay node library, in-place editing by default, multi-file atomic patching, runtime graph VM, independent graph behavior truth, or a stable public scripting/editor SDK.

## Change checklist

- Keep C# canonical and generated views derived.
- Preserve unsupported valid C# as opaque source.
- Separate projection, operation, preview, apply, compile, and runtime load.
- Require source root, hash, exact span, and supported lexical replacement.
- Preserve all bytes outside the approved span.
- Keep editor UI from writing C# directly.
- Keep graph evaluation and runtime-host placeholders out of the public Visual Blocks surface.
- Add fixture evidence for every newly accepted construct or operation.
- Repeat the no-graph-VM statement wherever a new visual surface could imply otherwise.
