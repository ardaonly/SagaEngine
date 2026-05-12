# Prism Tests

Lightweight tests for Prism's Python launcher and bootstrap-facing CLI contract.

These tests intentionally avoid LLVM and `compile_commands.json`; extractor
correctness belongs to fixture/golden tests once the Prism test corpus exists.

Run:

```bash
python3 Tools/Prism/tests/run.py
python3 Tools/Prism/build.py --run-tests
```
