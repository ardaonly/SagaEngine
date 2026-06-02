# SagaLaunchLab

SagaLaunchLab is the bounded local preview launcher for MultiplayerSandbox.

Phase 22 supports only the server-side headless preview path:

```sh
Tools/SagaLaunchLab/sagalaunch server --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --launch-profile local-server-headless --out /tmp/launch_preview_report.json --timeout-sec 5 --bin-dir build/RelWithDebInfo-0.0.9/bin
Tools/SagaLaunchLab/sagalaunch profile-matrix --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/launch_profile_matrix_report.json --bin-dir build/RelWithDebInfo-0.0.9/bin
```

Phase 25 acceptance records the supported server-only path and explicitly
records runtime client preview stages as deferred until a safe bounded
ClientHost seam exists.

The profile matrix keeps one-client and two-client preview rows deferred until
a bounded ClientHost/runtime preview report seam exists.
