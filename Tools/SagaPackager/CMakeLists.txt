saga_add_program(SagaPackagerProductIntegration
    MODULE_DIR Tools/SagaPackager
    TARGET SagaProductLib
    SOURCES
        ProductIntegration/SagaPackageStaging.cpp
        ProductIntegration/SagaPackageStaging.h
        ProductIntegration/SagaPublishReadiness.cpp
        ProductIntegration/SagaPublishReadiness.h
)
