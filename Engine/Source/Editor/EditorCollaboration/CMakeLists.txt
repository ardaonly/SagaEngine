saga_add_program(EditorCollaborationProductIntegration
    MODULE_DIR Engine/Source/Editor/EditorCollaboration
    TARGET SagaProductLib
    SOURCES
        Private/ProductIntegration/SagaLocalCollaborationMetadataReports.cpp
        Private/ProductIntegration/SagaLocalCollaborationMetadataReports.h
        Private/ProductIntegration/SagaLocalWorkspaceTransactionReport.cpp
        Private/ProductIntegration/SagaLocalWorkspaceTransactionReport.h
)

saga_add_editor_module(EditorCollaboration
    MODULE_DIR Engine/Source/Editor/EditorCollaboration
    TARGET SagaCollaboration
    SOURCES
        Private/SagaCollaboration/CollaborationModel.cpp
        Private/SagaCollaboration/CollaborationServices.cpp
        Private/SagaShared/CollaborationContracts.cpp
        Public/SagaCollaboration/ChangeStreamService.hpp
        Public/SagaCollaboration/ClaimService.hpp
        Public/SagaCollaboration/CollaborationBackend.hpp
        Public/SagaCollaboration/CollaborationModel.hpp
        Public/SagaCollaboration/CollaborationService.hpp
        Public/SagaCollaboration/ConflictService.hpp
        Public/SagaCollaboration/LockService.hpp
        Public/SagaCollaboration/PermissionService.hpp
        Public/SagaCollaboration/PresenceService.hpp
        Public/SagaCollaboration/SessionService.hpp
        Public/SagaShared/Collaboration/CollaborationDiagnostic.hpp
        Public/SagaShared/Collaboration/ParticipantId.hpp
        Public/SagaShared/Collaboration/PermissionGrant.hpp
        Public/SagaShared/Collaboration/PresenceSnapshot.hpp
        Public/SagaShared/Collaboration/ResourceClaim.hpp
        Public/SagaShared/Collaboration/ResourceLock.hpp
        Public/SagaShared/Collaboration/RoomCode.hpp
        Public/SagaShared/Collaboration/SessionDescriptor.hpp
        Public/SagaShared/Collaboration/SessionId.hpp
        Public/SagaShared/Workspace/WorkspaceDescriptor.hpp
        Public/SagaShared/Workspace/WorkspaceSyncState.hpp
)
