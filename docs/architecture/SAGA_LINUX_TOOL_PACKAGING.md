# Saga Linux Tool Packaging

> Status: Linux packaged tool launcher evidence contract

`scripts/package-linux-saga` stages `sagaproject`, `sagascript`, and `sagapack`
as packaged distribution tools instead of copying the developer-tree
`dotnet run --project` wrappers directly into the final archive.

## Current Closure

The repository wrappers remain developer-tree wrappers:

```txt
Tools/SagaProjectKit/sagaproject
Tools/SagaScript/sagascript
Tools/SagaPackager/sagapack
```

Those wrappers expect adjacent `.csproj` files and source trees. They are not
suitable as-is for the unpacked Linux distribution because the distribution does
not stage those source trees beside `Saga/tools/`.

This document publishes the real .NET CLI projects with framework-dependent
`dotnet publish` output:

```txt
Tools/SagaProjectKit/SagaProjectKit.csproj
Tools/SagaScript/SagaScript.csproj
Tools/SagaPackager/SagaPackager.csproj
```

The staged layout contains:

```txt
Saga/tools/sagaproject
Saga/tools/sagascript
Saga/tools/sagapack
Saga/tools/.saga-tools/sagaproject/
Saga/tools/.saga-tools/sagascript/
Saga/tools/.saga-tools/sagapack/
```

The visible `Saga/tools/<name>` files are generated distribution launchers. They
execute the packaged DLLs from `tools/.saga-tools/<name>/` through the discovered
.NET 10 command and do not fall back to repository paths or repository project
files.

## Evidence Boundary

The current contract smoke evidence proves only that these unpacked commands run:

```bash
/tmp/saga_dist_smoke/Saga/tools/sagaproject --help
/tmp/saga_dist_smoke/Saga/tools/sagascript --help
/tmp/saga_dist_smoke/Saga/tools/sagapack --help
```

It does not prove full tool workflows, StarterArena validation, full editor
workflow, Visual Blocks UI, production readiness, enterprise readiness, full
distribution validation, verified release status, or verified final release
status.

## Runtime Requirement

The published tools are framework-dependent `net10.0` applications. The
distribution launcher records the .NET 10 command discovered during staging and
also allows `SAGA_DOTNET` to point at another compatible .NET command. This is
not portability or production-readiness evidence.
