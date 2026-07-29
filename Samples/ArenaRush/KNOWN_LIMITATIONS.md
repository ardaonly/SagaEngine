# Known limitations

This source is a foundation contract, not a finished game. Main Menu, HUD, and Results are valid English RML/RCSS documents compiled through one `runtime.ui-set`; they do not yet receive runtime events or state.

Challenge-start and spawn entities are Transform-only authoring markers. The reusable managed contracts own deterministic values and transitions only; they do not own engine callbacks, clocks, collisions, score services, movement, scene changes, or pooling.
