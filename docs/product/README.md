# Product Documentation

> **Owner:** Product documentation
> **Audience:** Contributors, maintainers, and evaluators
> **Update trigger:** A current product document is added, removed, renamed, or changes authority
> **Authority:** Index only; linked current documents and accepted evidence remain authoritative

These documents define SagaEngine's current product language, current evidence
boundary, and selected long-term direction.

Product direction and current capability are intentionally separate.

## Start Here

1. [What Is SagaEngine?](WHAT_IS_SAGAENGINE.md)
   Current product definition, target specialization, open-source/service
   boundary, extension direction, and maturity boundary.

2. [Saga MMO Genre Focus](SAGA_MMO_GENRE_FOCUS.md)
   The selected Persistent Community Worlds direction, core/package boundary,
   creator trust model, and re-evaluation rule.

3. [Current Capabilities](CURRENT_CAPABILITIES.md)
   Bounded capabilities supported by current code and focused evidence.

4. [What Is Not Implemented](WHAT_IS_NOT_IMPLEMENTED.md)
   Current non-claims and incomplete product areas.

5. [Saga Ecosystem Map](SAGA_ECOSYSTEM_MAP.md)
   Component roles, ownership boundaries, artifact flow, and product
   specialization.

6. [Getting Started](GETTING_STARTED.md)
   Conservative entry point for reading and checking the repository.

## Additional Current Product References

- [Current Distribution Status](CURRENT_DISTRIBUTION_STATUS.md)
- [`.sagaproj` Schema v0](SAGAPROJ_SCHEMA_V0.md)


## Authority And Evidence Order

For implemented behavior, use this order:

1. current source code and build targets;
2. accepted automated tests, runtime evidence, and verified artifacts;
3. current product and architecture documentation;
4. private execution plans;
5. historical drafts, milestone notes, and archived strategy documents.

Documentation does not turn a planned behavior into an implemented capability.
Source code is not automatically the intended architecture when an ownership
conflict is already recorded; such conflicts must be resolved through consumer,
dependency, runtime, and evidence analysis.

## Related Current Indexes

- [Architecture Overview](../architecture/ARCHITECTURE_OVERVIEW.md)
- [Claim And Evidence Policy](../architecture/CLAIM_AND_EVIDENCE_POLICY.md)
- [Testing](../testing/README.md)

## Legal And Contribution Authority

Product documentation does not replace repository license files, notices,
third-party attribution, contributor terms, security policy, or any future
trademark policy.

Code provenance and contribution acceptance rules must live in the current
contribution or governance documentation. Product direction cannot authorize
the use of leaked, proprietary, or license-unclear source code.

## Documentation Rules

- Product claims start in this directory.
- Architecture documents define ownership and implementation boundaries, not
  product completion.
- Focused tests and reports prove only their declared slice.
- Positive current capability claims must remain traceable to current evidence.
- Historical plans, milestones, closure notes, candidate notes, and strategy
  drafts are background material unless a current document links to them for a
  bounded purpose.
- The Persistent Community Worlds direction is not a present-tense MMO
  readiness claim.
