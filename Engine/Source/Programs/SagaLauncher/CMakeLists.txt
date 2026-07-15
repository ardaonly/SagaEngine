saga_add_program(SagaLauncher
    MODULE_DIR Engine/Source/Programs/SagaLauncher
    TARGET SagaProductLib
    SOURCES
        Private/App/SagaApp.cpp
        Private/App/SagaApp.h
        Private/App/SagaAppConfig.cpp
        Private/App/SagaAppConfig.h
        Private/App/SagaProductHost.cpp
        Private/App/SagaProductHost.h
        Private/Launcher/SagaLauncherController.cpp
        Private/Launcher/SagaLauncherController.h
        Private/Launcher/SagaLauncherModel.cpp
        Private/Launcher/SagaLauncherModel.h
        Private/Launcher/SagaLauncherWindow.cpp
        Private/Launcher/SagaLauncherWindow.h
        Private/Processes/SagaProcessLauncher.cpp
        Private/Processes/SagaProcessLauncher.h
        Private/Processes/SagaProcessService.cpp
        Private/Processes/SagaProcessService.h
        Private/Projects/SagaProjectCatalog.cpp
        Private/Projects/SagaProjectCatalog.h
        Private/Projects/SagaProjectSystem.cpp
        Private/Projects/SagaProjectSystem.h
        Private/Projects/SagaSessionModel.cpp
        Private/Projects/SagaSessionModel.h
        Private/Projects/SagaWorkspaceResolver.cpp
        Private/Projects/SagaWorkspaceResolver.h
        Private/Reports/RuntimeEvidenceReport.cpp
        Private/Reports/RuntimeEvidenceReport.h
        Private/Reports/SagaLauncherReports.cpp
        Private/Reports/SagaLauncherReports.h
        Private/Reports/SagaProductWorkflowSmokeReport.cpp
        Private/Reports/SagaProductWorkflowSmokeReport.h
        Private/Targets/SagaLauncherTargets.cpp
        Private/Targets/SagaLauncherTargets.h
)

saga_add_program(SagaLauncher
    MODULE_DIR Engine/Source/Programs/SagaLauncher
    TARGET Saga
    SOURCES
        Private/App/SagaQtStaticPlugins.cpp
        Private/App/main.cpp
)
