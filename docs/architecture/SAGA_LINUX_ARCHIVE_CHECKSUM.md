# Saga Linux Archive And Checksum

> Status: Linux archive/checksum evidence contract

This document records archive and checksum evidence semantics. It is not a
release signing, maintainer verification, or readiness claim.

`scripts/package-linux-saga` creates the first Linux archive and checksum from
the staged layout:

```txt
build/dist/linux/Saga/
```

The generated outputs are:

```txt
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
```

These files are archive/checksum evidence only. They do not claim production
readiness, enterprise readiness, full distribution validation, verified release
status, verified final release status, maintainer verification, or workflow
correctness.

## Archive Contract

The archive is generated only from `build/dist/linux/Saga/`:

```bash
tar --zstd --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
  -cf build/dist/linux/Saga.tar.zst \
  -C build/dist/linux Saga
```

The archive root is `Saga/`. The archive must not include build caches, `.git`,
reports outside the staged layout, sibling archive/checksum files, or unrelated
artifacts.

If `tar` with `--zstd` support or the `zstd` executable is unavailable, the
script must block honestly and must not produce a different archive format.

## Checksum Contract

The checksum covers only `Saga.tar.zst` and is compatible with:

```bash
cd build/dist/linux
sha256sum -c Saga.sha256
```

The checksum file format is:

```txt
<sha256>  Saga.tar.zst
```

The checksum is not generated if archive generation fails.

## Unpack Smoke

This document adds `scripts/smoke-linux-saga-dist` as a separate unpack smoke. It
uses `Saga.tar.zst` as the distribution input, verifies `Saga.sha256`, unpacks
into a clean temporary directory, and runs limited checks from the unpacked
`Saga/` tree.

That smoke is still not production readiness, enterprise readiness, verified
final release status, full distribution verification, full editor workflow, full
Visual Blocks UI, or cloud collaboration.

## Non-Claims

Archive/checksum success does not claim:

- production readiness;
- enterprise readiness;
- verified release status;
- verified final release status;
- full distribution validation;
- full editor UI readiness;
- Visual Blocks editor UI readiness;
- cloud collaboration readiness;
- runtime workflow correctness;
- editor workflow correctness;
- server workflow correctness;
- tool workflow correctness beyond input existence checks;
- historical verified status.
