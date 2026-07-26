#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run all SagaProjectKit CLI tests in deterministic order."""

from cli_suite.test_validation import TESTS as VALIDATION_TESTS
from cli_suite.test_source_truth import TESTS as SOURCE_TRUTH_TESTS
from cli_suite.test_client_readiness import TESTS as CLIENT_READINESS_TESTS
from cli_suite.test_evaluation_plans import TESTS as EVALUATION_PLAN_TESTS
from cli_suite.test_templates import TESTS as TEMPLATE_TESTS

TESTS = (*VALIDATION_TESTS, *SOURCE_TRUTH_TESTS, *CLIENT_READINESS_TESTS, *EVALUATION_PLAN_TESTS, *TEMPLATE_TESTS)

def run_all() -> None:
    for test in TESTS:
        test()
    print(f"{len(TESTS)} SagaProjectKit CLI tests passed")

if __name__ == "__main__":
    run_all()
