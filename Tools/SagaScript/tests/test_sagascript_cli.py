#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run the complete SagaScript CLI contract suite in deterministic order."""

from __future__ import annotations

from cli_suite.test_analysis import TESTS as ANALYSIS_TESTS
from cli_suite.test_projection import TESTS as PROJECTION_TESTS
from cli_suite.test_block_operations import TESTS as BLOCK_OPERATION_TESTS
from cli_suite.test_patch_workflow import TESTS as PATCH_WORKFLOW_TESTS
from cli_suite.test_worker_protocol import TESTS as WORKER_PROTOCOL_TESTS
from cli_suite.test_module_usage import TESTS as MODULE_USAGE_TESTS

TESTS = (
    *WORKER_PROTOCOL_TESTS,
    *MODULE_USAGE_TESTS,
    *ANALYSIS_TESTS,
    *PROJECTION_TESTS,
    *BLOCK_OPERATION_TESTS,
    *PATCH_WORKFLOW_TESTS,
)


def main() -> None:
    for test in TESTS:
        test()
    print(f"{len(TESTS)} SagaScript CLI tests passed")


if __name__ == "__main__":
    main()
