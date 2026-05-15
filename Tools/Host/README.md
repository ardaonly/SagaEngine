# SagaEngine Host Helper

`Tools/Host` is a small Docker Compose wrapper for the local SagaEngine server stack.

Use it through SagaTools after running `Tools/SagaTools/setup.py`:

```bash
tools host start
tools host stop
tools host restart
tools host rebuild
tools host logs
tools host status
```

Direct invocation also works:

```bash
Tools/Host/host.sh start
```

The helper starts Postgres, Redis, and the local `SagaServer` container. It does
not delete Docker volumes during normal stop/restart operations.
