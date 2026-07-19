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
        Private/main.cpp
)
