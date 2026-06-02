# StarterArena Acceptance

Phase 10 acceptance is blocked. There is no accepted command today that launches
StarterArena as a runtime-backed local playable loop.

Future acceptance for the first minimal runtime loop requires all of the
following real evidence:

- a runtime launch command that consumes `StarterArena.sagaproj`;
- a minimal scene/resource loaded by that runtime command;
- a player spawn point;
- bounded local input-driven movement;
- simple viewport or camera behavior;
- bounds or collision behavior;
- restart and quit behavior when supported by the runtime;
- a smoke report, test, or CLI result proving the runtime path ran.

Documentation, metadata, or scene files alone are not enough to move Phase 10 to
`Implemented-Unverified`.
