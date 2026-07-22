# StarterArena

StarterArena is licensed under the MIT License; see `../LICENSE`.

StarterArena is the canonical seed and reference project for source, tooling, runtime, and authoring contracts.

Source truth consists of `StarterArena.sagaproj`, `Scenes/StarterArena.sagascene`, `Input/Input.saga.json`, `Scripts/DoorController.cs`, and the two primitive asset sources under `Assets/`. `SagaPackage.lock` is the only tracked GeneratedState exception. Cooked assets and all other build products belong under ignored `Build/`.

The scene connects logical action `Interact` to `DoorController.OnInteract`. Only the input map records the physical `Keyboard E` binding. The controller uses the existing SagaScript lifecycle and `Context.Self.Transform` facade.

See `ACCEPTANCE.md` for the editorless command sequence and `KNOWN_LIMITATIONS.md` for current non-claims.
