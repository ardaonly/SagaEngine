# Editor UI Implementation Strategy

Status: Policy checkpoint before Hedef 2 Block C and Phase 73 review work.

This document defines the editor UI boundary for upcoming alpha work. It does
not add Qt widgets, panels, product-shell wiring, source writes, or a public Qt
ABI expansion.

## Strategy Decision

The public editor authoring surface remains backend-neutral. Public models and
contracts must continue to use plain C++ data structures and report paths that
can be tested without Qt.

Qt implementation details may exist privately in later UI phases, but Qt types
must not be added to public editor authoring model headers.

## First Real UI Candidate

The first UI candidate should be a review and diff surface backed by existing
model/report contracts:

- project and artifact status;
- source links;
- projection report data;
- runtime binding report data;
- patch preview review data;
- diagnostics from generated reports.

This surface should make existing evidence readable. It should not become the
owner of source-changing behavior.

## Patch Workflow Boundary

Editor UI may:

- request patch previews through tool commands;
- show patch preview diagnostics;
- show source diff or span review;
- let a user cancel, close, or mark a review state;
- open the relevant report or source location.

Editor UI must not:

- write C# source directly;
- bypass SagaScript for future source-changing behavior;
- hide unsupported or opaque regions;
- present preview artifacts as already-written source changes;
- expand public Qt ABI to support this workflow.

## Public ABI Rule

`EditorQtPublicAbiBoundaryTests` remains the guard for public Qt exposure.
Future UI work should keep Qt includes and Qt-owned widgets inside private
implementation files unless a separate architecture checkpoint explicitly
changes the public boundary.

## Phase Boundary

Phase 73 may add review and diff model behavior if it remains report-backed.
Phase 76-81 may add editor workflow usability only after the backend-neutral
model boundary remains intact.
