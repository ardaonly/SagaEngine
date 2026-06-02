# Package Source Truth Alignment Phase 146

Phase 146 adds `sagapack source-truth-alignment`. The command reads package
profiles and supplied source-truth and asset-reference gate reports.

It reports package profiles, referenced launch profile ids, gate statuses, and
explicit rejection of package/generated manifests as canonical source truth.
It does not stage packages, rewrite package formats, or create package output.

Reports preserve local report-only invariants and keep package artifacts as
evidence only.
