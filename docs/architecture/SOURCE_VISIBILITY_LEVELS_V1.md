# Source Visibility Levels v1

This document models source visibility as a local tool/view classification. It
does not hide files from the filesystem, enforce identity, encrypt source, or
provide production source controls.

## Canonical Levels

The canonical visibility levels are:

- `FullSource`: source path may be reported and source-aware views may treat
  the artifact as visible.
- `SourceLinkOnly`: source text is not embedded; tools may report a link or
  project-relative reference.
- `SummaryOnly`: tools may report identity, kind, and status, but not source
  text.
- `OpaqueRestricted`: tools must use a placeholder and diagnostics.
- `Hidden`: tools omit the artifact from visible output and emit a diagnostic
  or count so hidden content is not silently erased.

`Missing`, `Invalid`, `Unknown`, `Included`, `Excluded`, and `Restricted` are
resolution statuses or diagnostics, not visibility levels.

## Resource Mapping

Visibility applies to:

- C# script files
- behavior entries
- node entries
- source spans
- asset paths
- package profiles
- launch profiles
- diagnostics reports

`FullSource` and `SourceLinkOnly` can expose project-relative source references.
`SummaryOnly` exposes metadata only. `OpaqueRestricted` and `Hidden` must avoid
source text and produce report diagnostics.

## View Compatibility

Simple View may show safe summaries and must disclose restricted or hidden
counts. Pro View may show diagnostics but must not expose restricted metadata.
CSharpSource View is compatible only with `FullSource` and `SourceLinkOnly`.
Diagnostics View may show visibility diagnostics.

## Non-Goals

This document does not implement auth, identity, role enforcement, source
encryption, direct filesystem protection, shared workspace service, Runtime
behavior, Server behavior, client preview behavior, Editor UI, or Qt UI.
