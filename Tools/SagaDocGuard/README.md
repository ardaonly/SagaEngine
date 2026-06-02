# SagaDocGuard

SagaDocGuard is the Phase 59 docs honesty scanner.

It performs deterministic string/regex checks over Markdown files, records
reviewed non-claim matches, checks required evidence files, and writes a report.
It does not use AI reasoning and does not rewrite documentation.

```sh
Tools/SagaDocGuard/sagadocguard scan --docs docs --evidence-root . --out Build/Reports/docguard_report.json
```
