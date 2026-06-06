# Saga Diligent Patch Policy

- Keep `Vendor/Diligent` limited to the DiligentCore fork for this migration.
- Make Saga-specific build integration changes in SagaEngine CMake whenever
  possible.
- Changes that must touch Diligent source belong in the Saga DiligentCore fork
  and should be described here with the fork commit hash and reason.
- Do not vendor DiligentFX, DiligentTools, samples, or extra Diligent
  repositories in this phase.

