# SagaEngine Host Helper

`Tools/Host` is a small Docker Compose wrapper for local development
dependencies.

Use it through SagaTools after running `Tools/SagaTools/setup.py`:

```bash
tools host start
tools host stop
tools host restart
tools host logs
tools host status
```

Direct invocation also works:

```bash
Tools/Host/host.sh start
```

The helper starts PostgreSQL and Redis. It does not launch an engine product
process and does not delete Docker volumes during normal stop/restart
operations.
