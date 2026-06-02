# Component Metadata Ownership Phase 141

Phase 141 is a local, report-only Hedef 4 slice. `sagaproject
component-metadata-ownership` reads scene/entity source truth, scene validation
evidence, and generated freshness evidence.

The command classifies `Transform` and `BehaviorReference` component metadata
from scene/entity JSON as `SceneEntitySourceTruth`. Unknown component types are
diagnosed. Generated runtime/script bindings and generated projections are
classified as `EvidenceOnly`; stale generated evidence is debt, not canonical
truth.

The command does not inspect or mutate C# source. Reports preserve
`localOnly=true`, `networkExposure=None`, `mutatesSource=false`, and
`enforcement=ReportOnly`.
