# Source Truth Scenario Phase 148

Phase 148 adds `sagaproject source-truth-scenario`. The command reads required
reports from `Build/SourceTruth` or a supplied evidence root.

Required evidence includes scene validation, asset reference gate, generated
freshness, component ownership, editor inspection/read, runtime audit/readiness,
package alignment, and launch alignment.

The scenario returns `PartiallyProven` when stale generated evidence and Client
Preview deferral are explicit. It returns `Blocked` when required
scene/entity/asset/component ownership evidence is missing. It performs no
package staging, launch execution, Runtime gameplay, Server gameplay, Client
Preview, asset import, asset cook, or source mutation.
