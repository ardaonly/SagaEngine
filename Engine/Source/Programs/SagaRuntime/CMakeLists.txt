# SPDX-License-Identifier: MPL-2.0
saga_add_program(SagaRuntime
    MODULE_DIR Engine/Source/Programs/SagaRuntime
    TARGET SagaRuntime
    SOURCES
        Private/RuntimeApplication.cpp
        Private/RuntimeApplication.h
        Private/RuntimeAssetStartupBootstrap.cpp
        Private/RuntimeAssetStartupBootstrap.hpp
        Private/RuntimeCommandLine.h
        Private/RuntimeHost.cpp
        Private/RuntimeHost.h
        Private/RuntimeLaunchSession.cpp
        Private/RuntimeLaunchSession.h
        Private/RuntimeArtifactSession.cpp
        Private/RuntimeArtifactSession.h
        Private/RuntimeRenderSession.cpp
        Private/RuntimeRenderSession.h
        Private/RuntimeScriptSession.cpp
        Private/RuntimeScriptSession.h
        Private/RuntimeEvidenceSession.cpp
        Private/RuntimeEvidenceSession.h
        Private/RuntimeFrameLoop.cpp
        Private/RuntimeFrameLoop.h
        Private/RuntimeGameplaySession.cpp
        Private/RuntimeGameplaySession.h
        Private/RuntimeStateStore.cpp
        Private/RuntimeStateStore.h
        Private/RuntimeTriggerSession.cpp
        Private/RuntimeTriggerSession.h
        Private/RuntimeUiPromptController.cpp
        Private/RuntimeUiPromptController.h
        Private/RuntimeUiSession.cpp
        Private/RuntimeUiSession.h
        Private/main.cpp
)
