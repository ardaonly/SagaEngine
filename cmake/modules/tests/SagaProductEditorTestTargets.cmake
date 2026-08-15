# SPDX-License-Identifier: MPL-2.0
    # --- Saga product tests -------------------------------------------------
    #
    # Keep product orchestration tests in a separate target so package-staging
    # checks can run without rebuilding every unit test.
    if(SAGA_PRODUCT_TEST_SOURCES)
        add_executable(SagaProductTests ${SAGA_PRODUCT_TEST_SOURCES})
        saga_link_thirdparty(SagaProductTests)
        target_link_libraries(SagaProductTests PRIVATE
            SagaProductLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaProductTests PRIVATE
            ${SAGA_TEST_INCLUDE_DIRS}
            ${SAGA_ROOT}/Engine/Source/Programs/SagaLauncher/Private
            ${SAGA_ROOT}/Engine/Source/Editor/EditorCollaboration/Private
        )
        target_compile_definitions(SagaProductTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaProductTests COMMAND SagaProductTests)
        set_tests_properties(SagaProductTests PROPERTIES LABELS "product;unit")
    endif()

    # --- Product Launcher Qt Widgets tests --------------------------------
    set(SAGA_LAUNCHER_UI_TEST_SOURCES
        "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaLauncherUiTests.cpp"
        "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaLauncherLifecycleTests.cpp"
        "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaLauncherControlledBlocksTests.cpp"
        "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaLauncherThreeWayParityTests.cpp")
    set(SAGA_WORK_MODES_UI_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaWorkModesUiTests.cpp")
    if(EXISTS "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaLauncherUiTests.cpp")
        add_executable(SagaLauncherUiTests
            ${SAGA_LAUNCHER_UI_TEST_SOURCES}
            ${SAGA_WORK_MODES_UI_TEST_SOURCE})
        saga_link_thirdparty(SagaLauncherUiTests)
        target_link_libraries(SagaLauncherUiTests PRIVATE
            SagaProductLib
            GTest::gtest
        )
        target_include_directories(SagaLauncherUiTests PRIVATE
            ${SAGA_TEST_INCLUDE_DIRS}
            ${SAGA_ROOT}/Engine/Source/Programs/SagaLauncher/Private
        )
        target_compile_definitions(SagaLauncherUiTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_BUILD_ROOT="${CMAKE_BINARY_DIR}"
        )
        add_dependencies(SagaLauncherUiTests SagaRuntime)
        if(TARGET SagaManagedToolRuntimeStage)
            add_dependencies(SagaLauncherUiTests SagaManagedToolRuntimeStage)
        endif()
        add_test(NAME SagaLauncherUiTests COMMAND SagaLauncherUiTests)
        set_tests_properties(SagaLauncherUiTests PROPERTIES
            LABELS "product;ui"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        include(GoogleTest)
        gtest_discover_tests(SagaLauncherUiTests
            TEST_PREFIX "SagaLauncherXcb."
            DISCOVERY_MODE POST_BUILD
            PROPERTIES
                LABELS "suite-product-xcb"
                ENVIRONMENT "QT_QPA_PLATFORM=xcb"
                RESOURCE_LOCK "saga-product-display"
                TIMEOUT 600)
    endif()

    # --- Editor authoring spine tests ---------------------------------------
    if(EXISTS "${EDITOR_AUTHORING_SPINE_TEST_SOURCE}")
        add_executable(EditorAuthoringSpineTests
            ${EDITOR_AUTHORING_SPINE_TEST_SOURCE}
        )
        saga_link_thirdparty(EditorAuthoringSpineTests)
        target_link_libraries(EditorAuthoringSpineTests PRIVATE
            SagaEditorLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(EditorAuthoringSpineTests PRIVATE
            ${SAGA_TEST_INCLUDE_DIRS}
        )
        target_compile_definitions(EditorAuthoringSpineTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME EditorAuthoringSpineTests
            COMMAND EditorAuthoringSpineTests)
        set_tests_properties(EditorAuthoringSpineTests PROPERTIES
            LABELS "unit;editor;authoring")
    endif()

    # --- Collaboration model tests ------------------------------------------
    if(EXISTS "${COLLABORATION_MODEL_TEST_SOURCE}")
        add_executable(CollaborationModelTests
            ${COLLABORATION_MODEL_TEST_SOURCE}
        )
        target_link_libraries(CollaborationModelTests PRIVATE
            SagaCollaboration
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(CollaborationModelTests PRIVATE
            ${SAGA_TEST_INCLUDE_DIRS}
        )
        target_compile_definitions(CollaborationModelTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME CollaborationModelTests
            COMMAND CollaborationModelTests)
        set_tests_properties(CollaborationModelTests PROPERTIES
            LABELS "unit;collaboration")
    endif()
