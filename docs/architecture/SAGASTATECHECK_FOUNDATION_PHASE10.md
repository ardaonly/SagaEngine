# SagaStateCheck Foundation Phase 10

Phase 10 adds a deterministic state validation foundation without claiming full
replication correctness or live server-wide authoritative state coverage.

## Repository Finding

`AuthoritativeMovementCore`, `AuthoritativeMovementInputAdapter`, and
`AuthoritativeMovementCommandIntake` expose point lookups such as
`GetActorPosition(EntityId)` and dirty-entity consumption, but they do not expose
a stable read-only enumerator for all authoritative movement records. Ownership
also has point lookup through `ActorOwnershipRegistry`, but not a deterministic
full snapshot API.

Because that seam is not present, Phase 10 keeps validation in
`Tools/SagaStateCheck` and does not widen movement authority or `ZoneServer`.

## Contracts

`StateSnapshot` owns:

- `schemaVersion`
- `snapshotId`
- `tick`
- ordered `entities`

`EntitySnapshot` owns:

- `entityId`
- `ownerClientId`
- `position`
- optional caller-supplied velocity

`EntityChecksum` is deterministic. It hashes schema version, entity id, owner
client id, quantized position, and optional quantized velocity. Float fields are
quantized at 1/1000 unit by default. Wall-clock time, random values, pointers,
and unordered container iteration are not checksum inputs.

`DesyncReport` records:

- `Passed` or `Failed`
- mismatch count
- missing entities
- extra entities
- ownership mismatches
- position mismatches
- checksum mismatches
- deterministic diagnostics

The JSON writer emits `state_check_report` data with canonical entity ordering.

## Evidence

Focused test target:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaStateCheckTests --output-on-failure --timeout 30 -j 1
```

`SagaStateCheckTests` covers matching snapshots, position mismatch, missing
entity, extra entity, ownership mismatch, empty snapshots, deterministic
insertion-order handling, and deterministic `state_check_report.json` shape.

## Future Required Seam

A future live server-state validation phase needs an official read-only
authoritative movement snapshot provider that enumerates entity id, owner client
id, position, and any safe movement fields in deterministic order.

## Non-Goals

This phase does not implement full replication validation, anti-cheat,
production desync recovery, large-world validation, real transport validation,
or a replication correctness claim.
