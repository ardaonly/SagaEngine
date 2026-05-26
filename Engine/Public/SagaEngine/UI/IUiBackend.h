/// @file IUiBackend.h
/// @brief Minimal backend-neutral runtime UI backend contract.

#pragma once

#include "SagaEngine/UI/IUiResourceProvider.h"
#include "SagaEngine/UI/UiTypes.h"

#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SagaEngine::UI
{

/// Abstract runtime UI backend seam used by runtime-facing code.
class IUiBackend
{
public:
    virtual ~IUiBackend() = default;

    /// Initialize backend resources and the default UI context.
    [[nodiscard]] virtual bool Initialize(const UiBackendConfig& config) = 0;

    /// Release all backend resources and loaded documents.
    virtual void Shutdown() = 0;

    /// Update backend viewport dimensions.
    virtual void SetViewport(UiViewport viewport) = 0;

    /// Load a UI document and return a backend-owned handle.
    [[nodiscard]] virtual UiDocumentHandle LoadDocument(
        const UiDocumentRequest& request) = 0;

    /// Unload a previously loaded document.
    [[nodiscard]] virtual bool UnloadDocument(UiDocumentHandle handle) = 0;

    /// Show a loaded document.
    [[nodiscard]] virtual bool ShowDocument(UiDocumentHandle handle) = 0;

    /// Hide a loaded document.
    [[nodiscard]] virtual bool HideDocument(UiDocumentHandle handle) = 0;

    /// Advance UI animations, layout, and backend state.
    virtual void Update(double deltaSeconds) = 0;

    /// Render the active UI context through the backend.
    [[nodiscard]] virtual bool Render() = 0;

    /// Forward one backend-neutral input event into the UI backend.
    [[nodiscard]] virtual bool SubmitInput(const UiInputEvent& event) = 0;

    /// Return diagnostics accumulated since the last clear.
    [[nodiscard]] virtual const std::vector<UiDiagnostic>& Diagnostics()
        const noexcept = 0;

    /// Clear accumulated diagnostics.
    virtual void ClearDiagnostics() noexcept = 0;

    /// Return summary data for the most recent render call.
    [[nodiscard]] virtual UiRenderStats LastRenderStats() const noexcept = 0;

protected:
    IUiBackend() = default;
    IUiBackend(const IUiBackend&) = delete;
    IUiBackend& operator=(const IUiBackend&) = delete;
};

/// Deterministic no-op UI backend for headless tests and construction checks.
class NullUiBackend final : public IUiBackend
{
public:
    [[nodiscard]] bool Initialize(const UiBackendConfig& config) override
    {
        if (initialized_)
        {
            AddDiagnostic(
                UiDiagnosticCode::BackendAlreadyInitialized,
                UiDiagnosticSeverity::Warning,
                "Null UI backend is already initialized.",
                {},
                UiDocumentHandle::kInvalid);
            return true;
        }

        config_ = config;
        initialized_ = true;
        return true;
    }

    void Shutdown() override
    {
        documents_.clear();
        diagnostics_.clear();
        stats_ = UiRenderStats{};
        initialized_ = false;
        nextHandle_ = 1;
    }

    void SetViewport(UiViewport viewport) override
    {
        config_.viewport = viewport;
    }

    [[nodiscard]] UiDocumentHandle LoadDocument(
        const UiDocumentRequest& request) override
    {
        if (!RequireInitialized())
        {
            return UiDocumentHandle::kInvalid;
        }

        const std::filesystem::path resolved = Resolve(request);
        if (resolved.empty())
        {
            if (request.UsesDocumentId())
            {
                return UiDocumentHandle::kInvalid;
            }

            AddDiagnostic(
                UiDiagnosticCode::DocumentPathEmpty,
                UiDiagnosticSeverity::Error,
                "UI document path is empty.",
                request.path,
                UiDocumentHandle::kInvalid);
            return UiDocumentHandle::kInvalid;
        }

        if (!std::filesystem::exists(resolved))
        {
            AddDiagnostic(
                UiDiagnosticCode::DocumentLoadFailed,
                UiDiagnosticSeverity::Error,
                "UI document does not exist.",
                resolved,
                UiDocumentHandle::kInvalid);
            return UiDocumentHandle::kInvalid;
        }

        const UiDocumentHandle handle =
            static_cast<UiDocumentHandle>(nextHandle_++);
        documents_.emplace(
            handle,
            DocumentState{resolved, request.showImmediately});
        return handle;
    }

    [[nodiscard]] bool UnloadDocument(UiDocumentHandle handle) override
    {
        if (!RequireInitialized())
        {
            return false;
        }

        const auto removed = documents_.erase(handle);
        if (removed == 0)
        {
            AddInvalidDocumentDiagnostic(handle);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool ShowDocument(UiDocumentHandle handle) override
    {
        DocumentState* document = FindDocument(handle);
        if (!document)
        {
            return false;
        }

        document->visible = true;
        return true;
    }

    [[nodiscard]] bool HideDocument(UiDocumentHandle handle) override
    {
        DocumentState* document = FindDocument(handle);
        if (!document)
        {
            return false;
        }

        document->visible = false;
        return true;
    }

    void Update(double deltaSeconds) override
    {
        (void)deltaSeconds;
        (void)RequireInitialized();
    }

    [[nodiscard]] bool Render() override
    {
        if (!RequireInitialized())
        {
            return false;
        }

        ++stats_.frameIndex;
        stats_.drawCallCount = 0;
        stats_.compiledGeometryCount = 0;
        stats_.textureCount = 0;
        return true;
    }

    [[nodiscard]] bool SubmitInput(const UiInputEvent& event) override
    {
        (void)event;
        return RequireInitialized();
    }

    [[nodiscard]] const std::vector<UiDiagnostic>& Diagnostics()
        const noexcept override
    {
        return diagnostics_;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnostics_.clear();
    }

    [[nodiscard]] UiRenderStats LastRenderStats() const noexcept override
    {
        return stats_;
    }

private:
    /// Minimal document state tracked by the null backend.
    struct DocumentState
    {
        std::filesystem::path path; ///< Resolved document path.
        bool visible = false; ///< Current visibility flag.
    };

    [[nodiscard]] std::filesystem::path Resolve(
        const UiDocumentRequest& request)
    {
        if (request.UsesDocumentId())
        {
            if (!config_.resourceProvider)
            {
                AddDiagnostic(
                    UiDiagnosticCode::DocumentLoadFailed,
                    UiDiagnosticSeverity::Error,
                    "UI document id cannot be resolved without a resource provider.",
                    {},
                    UiDocumentHandle::kInvalid);
                return {};
            }

            const UiResourceResolveResult result =
                config_.resourceProvider->ResolveDocument(request.documentId);
            if (!result.Succeeded())
            {
                AddDiagnostic(
                    UiDiagnosticCode::DocumentLoadFailed,
                    UiDiagnosticSeverity::Error,
                    result.diagnostic,
                    result.path,
                    UiDocumentHandle::kInvalid);
                return {};
            }
            return result.path;
        }

        const std::filesystem::path& path = request.path;
        if (path.empty() || path.is_absolute() || config_.assetRoot.empty())
        {
            return path;
        }

        return config_.assetRoot / path;
    }

    [[nodiscard]] bool RequireInitialized()
    {
        if (initialized_)
        {
            return true;
        }

        AddDiagnostic(
            UiDiagnosticCode::BackendNotInitialized,
            UiDiagnosticSeverity::Error,
            "UI backend is not initialized.",
            {},
            UiDocumentHandle::kInvalid);
        return false;
    }

    [[nodiscard]] DocumentState* FindDocument(UiDocumentHandle handle)
    {
        if (!RequireInitialized())
        {
            return nullptr;
        }

        const auto it = documents_.find(handle);
        if (it == documents_.end())
        {
            AddInvalidDocumentDiagnostic(handle);
            return nullptr;
        }
        return &it->second;
    }

    void AddInvalidDocumentDiagnostic(UiDocumentHandle handle)
    {
        AddDiagnostic(
            UiDiagnosticCode::InvalidDocumentHandle,
            UiDiagnosticSeverity::Error,
            "UI document handle is not loaded.",
            {},
            handle);
    }

    void AddDiagnostic(
        UiDiagnosticCode code,
        UiDiagnosticSeverity severity,
        std::string message,
        std::filesystem::path path,
        UiDocumentHandle document)
    {
        diagnostics_.push_back(UiDiagnostic{
            code,
            severity,
            std::move(message),
            std::move(path),
            document,
        });
    }

    UiBackendConfig config_;
    std::unordered_map<UiDocumentHandle, DocumentState> documents_;
    std::vector<UiDiagnostic> diagnostics_;
    UiRenderStats stats_;
    std::uint32_t nextHandle_ = 1;
    bool initialized_ = false;
};

} // namespace SagaEngine::UI
