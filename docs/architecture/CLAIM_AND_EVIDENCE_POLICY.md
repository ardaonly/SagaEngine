# Claim And Evidence Policy

> Status: Current architecture policy
> Scope: Documentation claim vocabulary and evidence boundaries

SagaEngine documentation must not make the project look more complete than the
current evidence supports.

## Claim Vocabulary

| Term | Meaning | Allowed Use |
| --- | --- | --- |
| `Current` | Describes the repo as it exists now. | Only when supported by current product docs, current architecture summaries, source, or focused test/report evidence. |
| `Planning Note` | Describes intended future work. | Direction and planning only; not shipped behavior. |
| `Historical Draft` | Preserved context from earlier planning. | Background only; not current product promise or architecture source of truth. |
| `Candidate` | A bounded artifact or status under evaluation. | Must include limitations and must not imply beta, release, or production readiness. |
| `Implemented-Unverified` | Code, tool, or document work exists, but maintainer verification or accepted gate evidence is missing. | May be referenced as local evidence only, with the unverified status preserved. |
| `Report-Only Evidence` | A tool or document reports state without enforcing the state as a hard gate. | Useful for review, not proof that the system blocks or guarantees the behavior. |
| `Foundation` | A bounded base for later work. | Must name what is covered and what remains open. |
| `Production-Ready Forbidden` | A claim family that is not allowed for current SagaEngine docs. | Use only as a negative claim unless future evidence and product docs explicitly change the status. |

## Required Rules

- Product claims must start from `docs/product/`.
- Architecture claims must start from current summaries in `docs/architecture/`.
- Product slice notes and proposed architecture notes describe direction and
  backlog context; they do not prove shipped capability by themselves.
- Historical drafts and internal planning docs do not supersede current product
  or architecture summaries.
- A candidate status must name its limitations.
- A foundation status must not be shortened into a complete feature claim.
- Report-only evidence must not be described as enforcement.
- Implemented-unverified work must not be described as verified.

## Forbidden Positive Claims

These claims are forbidden as positive current claims unless a later current
product document and current architecture summary explicitly accept them:

- production-ready;
- enterprise-ready;
- release candidate;
- product beta;
- full SDK;
- complete editor workflow;
- complete visual scripting product;
- complete runtime gameplay loop;
- complete server gameplay loop;
- production networking or MMO-scale readiness;
- production renderer or visual output guarantee;
- production asset import/cook pipeline;
- full security, cloud, account, permission, or compliance model;
- release pipeline, installer, updater, marketplace, signing, or production
  distribution readiness;
- remote telemetry, production observability, crash safety, OS memory profiling,
  leak detection, benchmark suite, or performance readiness.

Negative wording is allowed when it clearly says the claim is not implemented,
blocked, deferred, or not claimed.

## Evidence Discipline

Evidence must say what it proves and what it does not prove. A focused test,
local report, smoke check, generated matrix, or package candidate may support a
bounded subsystem claim. It does not automatically support product readiness.

When a document uses `candidate`, `foundation`, `implemented`,
`proof`, `accepted`, or `closed`, it must preserve the scope and non-claims that
made the word safe.
