/// @file UiTypes.h
/// @brief Backend-neutral runtime UI value types and diagnostics.

#pragma once

#include "SagaEngine/Input/Types/InputTypes.h"
#include "SagaEngine/UI/UiResourceIds.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace SagaEngine::UI
{

class IUiResourceProvider;
class IUiEventSink;

/// Opaque runtime UI document handle. Zero is reserved for invalid handles.
enum class UiDocumentHandle : std::uint32_t
{
    kInvalid = 0,
};

/// Return true when a document handle was produced by a UI backend.
[[nodiscard]] constexpr bool IsValid(UiDocumentHandle handle) noexcept
{
    return handle != UiDocumentHandle::kInvalid;
}

/// Runtime UI viewport dimensions in physical pixels.
struct UiViewport
{
    std::uint32_t width = 0;     ///< Viewport width in pixels.
    std::uint32_t height = 0;    ///< Viewport height in pixels.
    float pixelRatio = 1.0f;     ///< Native display scale used by the backend.
};

/// Stable runtime UI screen id.
struct UiScreenId
{
    std::string value;

    [[nodiscard]] static UiScreenId FromString(std::string value)
    {
        return UiScreenId{std::move(value)};
    }

    [[nodiscard]] bool IsSet() const noexcept
    {
        return !value.empty();
    }
};

/// Return true when two screen ids refer to the same registered screen.
[[nodiscard]] inline bool operator==(
    const UiScreenId& lhs,
    const UiScreenId& rhs) noexcept
{
    return lhs.value == rhs.value;
}

/// UI backend startup configuration.
struct UiBackendConfig
{
    UiViewport viewport;              ///< Initial UI viewport.
    std::filesystem::path assetRoot;  ///< Optional root for relative document paths.
    std::shared_ptr<const IUiResourceProvider>
        resourceProvider; ///< Logical UI resource resolver.
    std::shared_ptr<IUiEventSink> eventSink; ///< Optional backend-neutral UI event sink.
    std::string contextName = "SagaRuntimeUi"; ///< Backend context name for diagnostics.
};

/// Document load request.
struct UiDocumentRequest
{
    UiScreenId screenId; ///< Runtime screen context for backend-emitted events.
    UiDocumentId documentId; ///< Preferred logical UI document id.
    std::filesystem::path path; ///< Low-level/debug direct document path fallback.
    bool showImmediately = true; ///< Show the document after a successful load.

    [[nodiscard]] static UiDocumentRequest FromDocumentId(
        UiDocumentId id,
        bool showImmediately = true,
        UiScreenId screenId = {})
    {
        UiDocumentRequest request;
        request.screenId = std::move(screenId);
        request.documentId = std::move(id);
        request.showImmediately = showImmediately;
        return request;
    }

    [[nodiscard]] static UiDocumentRequest FromDebugPath(
        std::filesystem::path path,
        bool showImmediately = true,
        UiScreenId screenId = {})
    {
        UiDocumentRequest request;
        request.screenId = std::move(screenId);
        request.path = std::move(path);
        request.showImmediately = showImmediately;
        return request;
    }

    [[nodiscard]] bool UsesDocumentId() const noexcept
    {
        return documentId.IsSet();
    }
};

/// UI diagnostic severity kept local to the minimal runtime UI contract.
enum class UiDiagnosticSeverity : std::uint8_t
{
    Info = 0,
    Warning = 1,
    Error = 2,
};

/// Stable diagnostic codes for the current minimal UI backend slice.
enum class UiDiagnosticCode : std::uint8_t
{
    None = 0,
    BackendAlreadyInitialized,
    BackendInitializeFailed,
    BackendNotInitialized,
    DocumentPathEmpty,
    DocumentLoadFailed,
    InvalidDocumentHandle,
    InputRejected,
    UnsupportedInput,
    RenderFailed,
    BackendMessage,
};

/// A structured UI diagnostic emitted by the active backend.
struct UiDiagnostic
{
    UiDiagnosticCode code = UiDiagnosticCode::None; ///< Stable diagnostic code.
    UiDiagnosticSeverity severity = UiDiagnosticSeverity::Info; ///< Diagnostic severity.
    std::string message; ///< Human-readable diagnostic detail.
    std::filesystem::path path; ///< Related document or asset path when available.
    UiDocumentHandle document = UiDocumentHandle::kInvalid; ///< Related document.
};

/// Runtime UI input event kind.
enum class UiInputEventType : std::uint8_t
{
    Key = 0,
    Text,
    MouseMove,
    MouseButton,
    MouseWheel,
};

/// Backend-neutral input event forwarded from Saga input to the UI backend.
struct UiInputEvent
{
    UiInputEventType type = UiInputEventType::Key; ///< Event discriminator.
    Input::ModifierFlags modifiers = Input::ModifierFlags::None; ///< Active modifiers.
    Input::KeyCode key = Input::KeyCode::Unknown; ///< Key for key events.
    Input::MouseButton mouseButton = Input::MouseButton::Unknown; ///< Button for mouse events.
    bool pressed = false; ///< Pressed state for key and mouse button events.
    bool repeat = false; ///< OS key repeat marker for key events.
    std::int32_t x = 0; ///< Mouse x coordinate.
    std::int32_t y = 0; ///< Mouse y coordinate.
    float wheelX = 0.0f; ///< Horizontal wheel delta.
    float wheelY = 0.0f; ///< Vertical wheel delta.
    char textUtf8[5]{}; ///< Null-terminated UTF-8 text input payload.
};

/// Last-render summary exposed without leaking backend geometry types.
struct UiRenderStats
{
    std::uint64_t frameIndex = 0; ///< Number of render calls accepted.
    std::size_t drawCallCount = 0; ///< Backend draw submissions in the last frame.
    std::size_t compiledGeometryCount = 0; ///< Live compiled geometry handles.
    std::size_t textureCount = 0; ///< Live generated or loaded texture handles.
};

} // namespace SagaEngine::UI
