/// @file RmlUiUiBackend.cpp
/// @brief RmlUi implementation of the minimal Saga runtime UI backend.

#include "RmlUiUiBackend.h"

#include "SagaEngine/UI/IUiEventQueue.h"
#include "SagaEngine/UI/IUiResourceProvider.h"
#include "SagaEngine/UI/UiTypes.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SagaEngine::UI::Backends
{
namespace
{

namespace SagaUI = ::SagaEngine::UI;
namespace SagaInput = ::SagaEngine::Input;

bool g_rmlUiOwned = false;

[[nodiscard]] std::filesystem::path ResolvePath(
    const std::filesystem::path& root,
    const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || root.empty())
    {
        return path;
    }

    return root / path;
}

[[nodiscard]] int ToRmlModifiers(SagaInput::ModifierFlags modifiers) noexcept
{
    int result = 0;
    if (SagaInput::HasModifier(modifiers, SagaInput::ModifierFlags::Ctrl))
    {
        result |= Rml::Input::KM_CTRL;
    }
    if (SagaInput::HasModifier(modifiers, SagaInput::ModifierFlags::Shift))
    {
        result |= Rml::Input::KM_SHIFT;
    }
    if (SagaInput::HasModifier(modifiers, SagaInput::ModifierFlags::Alt))
    {
        result |= Rml::Input::KM_ALT;
    }
    if (SagaInput::HasModifier(modifiers, SagaInput::ModifierFlags::Meta))
    {
        result |= Rml::Input::KM_META;
    }
    return result;
}

[[nodiscard]] int ToRmlMouseButton(SagaInput::MouseButton button) noexcept
{
    switch (button)
    {
    case SagaInput::MouseButton::Left:
        return 0;
    case SagaInput::MouseButton::Right:
        return 1;
    case SagaInput::MouseButton::Middle:
        return 2;
    default:
        return -1;
    }
}

[[nodiscard]] Rml::Input::KeyIdentifier ToRmlKey(
    SagaInput::KeyCode key) noexcept
{
    using SagaInput::KeyCode;
    switch (key)
    {
    case KeyCode::A: return Rml::Input::KI_A;
    case KeyCode::B: return Rml::Input::KI_B;
    case KeyCode::C: return Rml::Input::KI_C;
    case KeyCode::D: return Rml::Input::KI_D;
    case KeyCode::E: return Rml::Input::KI_E;
    case KeyCode::F: return Rml::Input::KI_F;
    case KeyCode::G: return Rml::Input::KI_G;
    case KeyCode::H: return Rml::Input::KI_H;
    case KeyCode::I: return Rml::Input::KI_I;
    case KeyCode::J: return Rml::Input::KI_J;
    case KeyCode::K: return Rml::Input::KI_K;
    case KeyCode::L: return Rml::Input::KI_L;
    case KeyCode::M: return Rml::Input::KI_M;
    case KeyCode::N: return Rml::Input::KI_N;
    case KeyCode::O: return Rml::Input::KI_O;
    case KeyCode::P: return Rml::Input::KI_P;
    case KeyCode::Q: return Rml::Input::KI_Q;
    case KeyCode::R: return Rml::Input::KI_R;
    case KeyCode::S: return Rml::Input::KI_S;
    case KeyCode::T: return Rml::Input::KI_T;
    case KeyCode::U: return Rml::Input::KI_U;
    case KeyCode::V: return Rml::Input::KI_V;
    case KeyCode::W: return Rml::Input::KI_W;
    case KeyCode::X: return Rml::Input::KI_X;
    case KeyCode::Y: return Rml::Input::KI_Y;
    case KeyCode::Z: return Rml::Input::KI_Z;
    case KeyCode::Num0: return Rml::Input::KI_0;
    case KeyCode::Num1: return Rml::Input::KI_1;
    case KeyCode::Num2: return Rml::Input::KI_2;
    case KeyCode::Num3: return Rml::Input::KI_3;
    case KeyCode::Num4: return Rml::Input::KI_4;
    case KeyCode::Num5: return Rml::Input::KI_5;
    case KeyCode::Num6: return Rml::Input::KI_6;
    case KeyCode::Num7: return Rml::Input::KI_7;
    case KeyCode::Num8: return Rml::Input::KI_8;
    case KeyCode::Num9: return Rml::Input::KI_9;
    case KeyCode::Escape: return Rml::Input::KI_ESCAPE;
    case KeyCode::Enter: return Rml::Input::KI_RETURN;
    case KeyCode::Backspace: return Rml::Input::KI_BACK;
    case KeyCode::Tab: return Rml::Input::KI_TAB;
    case KeyCode::Space: return Rml::Input::KI_SPACE;
    case KeyCode::Delete: return Rml::Input::KI_DELETE;
    case KeyCode::Insert: return Rml::Input::KI_INSERT;
    case KeyCode::Home: return Rml::Input::KI_HOME;
    case KeyCode::End: return Rml::Input::KI_END;
    case KeyCode::PageUp: return Rml::Input::KI_PRIOR;
    case KeyCode::PageDown: return Rml::Input::KI_NEXT;
    case KeyCode::Left: return Rml::Input::KI_LEFT;
    case KeyCode::Right: return Rml::Input::KI_RIGHT;
    case KeyCode::Up: return Rml::Input::KI_UP;
    case KeyCode::Down: return Rml::Input::KI_DOWN;
    case KeyCode::F1: return Rml::Input::KI_F1;
    case KeyCode::F2: return Rml::Input::KI_F2;
    case KeyCode::F3: return Rml::Input::KI_F3;
    case KeyCode::F4: return Rml::Input::KI_F4;
    case KeyCode::F5: return Rml::Input::KI_F5;
    case KeyCode::F6: return Rml::Input::KI_F6;
    case KeyCode::F7: return Rml::Input::KI_F7;
    case KeyCode::F8: return Rml::Input::KI_F8;
    case KeyCode::F9: return Rml::Input::KI_F9;
    case KeyCode::F10: return Rml::Input::KI_F10;
    case KeyCode::F11: return Rml::Input::KI_F11;
    case KeyCode::F12: return Rml::Input::KI_F12;
    default:
        return Rml::Input::KI_UNKNOWN;
    }
}

/// Append UI diagnostics for backend-owned failure paths.
class DiagnosticCollector
{
public:
    explicit DiagnosticCollector(std::vector<SagaUI::UiDiagnostic>& diagnostics)
        : diagnostics_(diagnostics)
    {
    }

    void Add(
        SagaUI::UiDiagnosticCode code,
        SagaUI::UiDiagnosticSeverity severity,
        std::string message,
        std::filesystem::path path = {},
        SagaUI::UiDocumentHandle document = SagaUI::UiDocumentHandle::kInvalid)
    {
        diagnostics_.push_back(SagaUI::UiDiagnostic{
            code,
            severity,
            std::move(message),
            std::move(path),
            document,
        });
    }

private:
    std::vector<SagaUI::UiDiagnostic>& diagnostics_;
};

/// RmlUi system interface bridge for time and log diagnostics.
class RmlUiSystemInterface final : public Rml::SystemInterface
{
public:
    explicit RmlUiSystemInterface(DiagnosticCollector& diagnostics)
        : diagnostics_(diagnostics),
          start_(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] double GetElapsedTime() override
    {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_).count();
    }

    [[nodiscard]] bool LogMessage(
        Rml::Log::Type type,
        const Rml::String& message) override
    {
        SagaUI::UiDiagnosticSeverity severity = SagaUI::UiDiagnosticSeverity::Info;
        if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
        {
            severity = SagaUI::UiDiagnosticSeverity::Error;
        }
        else if (type == Rml::Log::LT_WARNING)
        {
            severity = SagaUI::UiDiagnosticSeverity::Warning;
        }

        diagnostics_.Add(
            SagaUI::UiDiagnosticCode::BackendMessage,
            severity,
            message);
        return true;
    }

private:
    DiagnosticCollector& diagnostics_;
    std::chrono::steady_clock::time_point start_;
};

/// Rooted RmlUi file interface for RML and RCSS document loads.
class RmlUiFileInterface final : public Rml::FileInterface
{
public:
    explicit RmlUiFileInterface(std::filesystem::path root)
        : root_(std::move(root))
    {
    }

    [[nodiscard]] Rml::FileHandle Open(const Rml::String& path) override
    {
        const std::filesystem::path resolved = ResolvePath(root_, path);
        std::FILE* file = std::fopen(resolved.string().c_str(), "rb");
        return reinterpret_cast<Rml::FileHandle>(file);
    }

    void Close(Rml::FileHandle file) override
    {
        if (file)
        {
            std::fclose(reinterpret_cast<std::FILE*>(file));
        }
    }

    [[nodiscard]] std::size_t Read(
        void* buffer,
        std::size_t size,
        Rml::FileHandle file) override
    {
        if (!file)
        {
            return 0;
        }

        return std::fread(buffer, 1, size, reinterpret_cast<std::FILE*>(file));
    }

    [[nodiscard]] bool Seek(
        Rml::FileHandle file,
        long offset,
        int origin) override
    {
        if (!file)
        {
            return false;
        }

        return std::fseek(reinterpret_cast<std::FILE*>(file), offset, origin) == 0;
    }

    [[nodiscard]] std::size_t Tell(Rml::FileHandle file) override
    {
        if (!file)
        {
            return 0;
        }

        const long position = std::ftell(reinterpret_cast<std::FILE*>(file));
        return position < 0 ? 0 : static_cast<std::size_t>(position);
    }

private:
    std::filesystem::path root_;
};

/// Counting render interface used until Saga UI is wired to a GPU backend.
class RmlUiCountingRenderInterface final : public Rml::RenderInterface
{
public:
    void BeginFrame()
    {
        frameStats_.drawCallCount = 0;
        frameStats_.compiledGeometryCount = geometry_.size();
        frameStats_.textureCount = textures_.size();
    }

    [[nodiscard]] SagaUI::UiRenderStats EndFrame()
    {
        frameStats_.compiledGeometryCount = geometry_.size();
        frameStats_.textureCount = textures_.size();
        return frameStats_;
    }

    void RenderGeometry(
        Rml::Vertex* vertices,
        int numVertices,
        int* indices,
        int numIndices,
        Rml::TextureHandle texture,
        const Rml::Vector2f& translation) override
    {
        (void)vertices;
        (void)numVertices;
        (void)indices;
        (void)numIndices;
        (void)translation;
        (void)texture;
        ++frameStats_.drawCallCount;
    }

    [[nodiscard]] Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Vertex* vertices,
        int numVertices,
        int* indices,
        int numIndices,
        Rml::TextureHandle texture) override
    {
        (void)vertices;
        (void)indices;
        (void)texture;

        const Rml::CompiledGeometryHandle handle = nextGeometryHandle_++;
        geometry_.emplace(
            handle,
            GeometryStats{
                static_cast<std::size_t>(std::max(numVertices, 0)),
                static_cast<std::size_t>(std::max(numIndices, 0)),
            });
        return handle;
    }

    void RenderCompiledGeometry(
        Rml::CompiledGeometryHandle geometry,
        const Rml::Vector2f& translation) override
    {
        (void)geometry;
        (void)translation;
        ++frameStats_.drawCallCount;
    }

    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override
    {
        geometry_.erase(geometry);
    }

    [[nodiscard]] bool LoadTexture(
        Rml::TextureHandle& textureHandle,
        Rml::Vector2i& textureDimensions,
        const Rml::String& source) override
    {
        textureHandle = 0;
        (void)source;
        textureDimensions = Rml::Vector2i(0, 0);
        return false;
    }

    [[nodiscard]] bool GenerateTexture(
        Rml::TextureHandle& textureHandle,
        const Rml::byte* source,
        const Rml::Vector2i& sourceDimensions) override
    {
        (void)source;
        textureHandle = nextTextureHandle_++;
        textures_.emplace(
            textureHandle,
            TextureStats{
                static_cast<std::uint32_t>(std::max(sourceDimensions.x, 0)),
                static_cast<std::uint32_t>(std::max(sourceDimensions.y, 0)),
            });
        return true;
    }

    void ReleaseTexture(Rml::TextureHandle texture) override
    {
        textures_.erase(texture);
    }

    void EnableScissorRegion(bool enable) override
    {
        scissorEnabled_ = enable;
    }

    void SetScissorRegion(int x, int y, int width, int height) override
    {
        scissorX_ = x;
        scissorY_ = y;
        scissorWidth_ = width;
        scissorHeight_ = height;
    }

private:
    /// Count of immutable geometry supplied by RmlUi.
    struct GeometryStats
    {
        std::size_t vertexCount = 0; ///< Vertex span size.
        std::size_t indexCount = 0; ///< Index span size.
    };

    /// Dimensions of a generated texture handle.
    struct TextureStats
    {
        std::uint32_t width = 0; ///< Texture width.
        std::uint32_t height = 0; ///< Texture height.
    };

    std::unordered_map<Rml::CompiledGeometryHandle, GeometryStats> geometry_;
    std::unordered_map<Rml::TextureHandle, TextureStats> textures_;
    SagaUI::UiRenderStats frameStats_;
    Rml::CompiledGeometryHandle nextGeometryHandle_ = 1;
    Rml::TextureHandle nextTextureHandle_ = 1;
    int scissorX_ = 0;
    int scissorY_ = 0;
    int scissorWidth_ = 0;
    int scissorHeight_ = 0;
    bool scissorEnabled_ = false;
};

/// RmlUi-backed implementation hidden behind the Saga UI backend contract.
class RmlUiBackend final : public SagaUI::IUiBackend
{
public:
    ~RmlUiBackend() override
    {
        Shutdown();
    }

    [[nodiscard]] bool Initialize(const SagaUI::UiBackendConfig& config) override
    {
        if (initialized_)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::BackendAlreadyInitialized,
                SagaUI::UiDiagnosticSeverity::Warning,
                "RmlUi backend is already initialized.");
            return true;
        }

        if (g_rmlUiOwned)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::BackendInitializeFailed,
                SagaUI::UiDiagnosticSeverity::Error,
                "Another RmlUi backend instance already owns RmlUi lifetime.");
            return false;
        }

        config_ = config;
        fileInterface_ =
            std::make_unique<RmlUiFileInterface>(config_.assetRoot);
        systemInterface_ =
            std::make_unique<RmlUiSystemInterface>(diagnostics_);

        Rml::SetFileInterface(fileInterface_.get());
        Rml::SetRenderInterface(&renderInterface_);
        Rml::SetSystemInterface(systemInterface_.get());

        if (!Rml::Initialise())
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::BackendInitializeFailed,
                SagaUI::UiDiagnosticSeverity::Error,
                "RmlUi failed to initialize.");
            Rml::SetFileInterface(nullptr);
            Rml::SetRenderInterface(nullptr);
            Rml::SetSystemInterface(nullptr);
            systemInterface_.reset();
            fileInterface_.reset();
            return false;
        }

        g_rmlUiOwned = true;

        const Rml::Vector2i dimensions(
            static_cast<int>(config_.viewport.width),
            static_cast<int>(config_.viewport.height));
        context_ = Rml::CreateContext(config_.contextName.c_str(), dimensions);
        if (!context_)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::BackendInitializeFailed,
                SagaUI::UiDiagnosticSeverity::Error,
                "RmlUi failed to create the runtime UI context.");
            Shutdown();
            return false;
        }

        AttachEventListeners();
        initialized_ = true;
        return true;
    }

    void Shutdown() override
    {
        if (!initialized_ && !g_rmlUiOwned)
        {
            return;
        }

        RemoveEventListeners();

        for (auto& [handle, document] : documents_)
        {
            (void)handle;
            if (document.document)
            {
                document.document->Close();
            }
        }
        documents_.clear();
        context_ = nullptr;

        if (g_rmlUiOwned)
        {
            Rml::Shutdown();
            Rml::SetFileInterface(nullptr);
            Rml::SetRenderInterface(nullptr);
            Rml::SetSystemInterface(nullptr);
            g_rmlUiOwned = false;
        }

        systemInterface_.reset();
        fileInterface_.reset();
        initialized_ = false;
        nextHandle_ = 1;
        lastRenderStats_ = SagaUI::UiRenderStats{};
    }

    void SetViewport(SagaUI::UiViewport viewport) override
    {
        config_.viewport = viewport;
        if (context_)
        {
            context_->SetDimensions(Rml::Vector2i(
                static_cast<int>(viewport.width),
                static_cast<int>(viewport.height)));
        }
    }

    [[nodiscard]] SagaUI::UiDocumentHandle LoadDocument(
        const SagaUI::UiDocumentRequest& request) override
    {
        if (!RequireInitialized())
        {
            return SagaUI::UiDocumentHandle::kInvalid;
        }

        const std::filesystem::path resolved = Resolve(request);
        if (resolved.empty())
        {
            if (request.UsesDocumentId())
            {
                return SagaUI::UiDocumentHandle::kInvalid;
            }

            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::DocumentPathEmpty,
                SagaUI::UiDiagnosticSeverity::Error,
                "UI document path is empty.");
            return SagaUI::UiDocumentHandle::kInvalid;
        }

        Rml::ElementDocument* document =
            context_->LoadDocument(resolved.generic_string().c_str());
        if (!document)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::DocumentLoadFailed,
                SagaUI::UiDiagnosticSeverity::Error,
                "RmlUi failed to load UI document.",
                resolved);
            return SagaUI::UiDocumentHandle::kInvalid;
        }

        const SagaUI::UiDocumentHandle handle =
            static_cast<SagaUI::UiDocumentHandle>(nextHandle_++);
        DocumentState loadedDocument;
        loadedDocument.document = document;
        loadedDocument.path = resolved;
        documents_.emplace(handle, std::move(loadedDocument));
        DocumentState& state = documents_.at(handle);
        state.screenId = request.screenId;
        state.documentId = request.documentId;
        state.handle = handle;

        if (request.showImmediately)
        {
            document->Show();
        }

        return handle;
    }

    [[nodiscard]] bool UnloadDocument(
        SagaUI::UiDocumentHandle handle) override
    {
        DocumentState* document = FindDocument(handle);
        if (!document)
        {
            return false;
        }

        document->document->Close();
        documents_.erase(handle);
        return true;
    }

    [[nodiscard]] bool ShowDocument(SagaUI::UiDocumentHandle handle) override
    {
        DocumentState* document = FindDocument(handle);
        if (!document)
        {
            return false;
        }

        document->document->Show();
        return true;
    }

    [[nodiscard]] bool HideDocument(SagaUI::UiDocumentHandle handle) override
    {
        DocumentState* document = FindDocument(handle);
        if (!document)
        {
            return false;
        }

        document->document->Hide();
        return true;
    }

    void Update(double deltaSeconds) override
    {
        (void)deltaSeconds;
        if (RequireInitialized())
        {
            context_->Update();
        }
    }

    [[nodiscard]] bool Render() override
    {
        if (!RequireInitialized())
        {
            return false;
        }

        renderInterface_.BeginFrame();
        context_->Render();
        lastRenderStats_ = renderInterface_.EndFrame();
        ++lastRenderStats_.frameIndex;
        return true;
    }

    [[nodiscard]] bool SubmitInput(const SagaUI::UiInputEvent& event) override
    {
        if (!RequireInitialized())
        {
            return false;
        }

        const int modifiers = ToRmlModifiers(event.modifiers);
        switch (event.type)
        {
        case SagaUI::UiInputEventType::Key:
            return SubmitKey(event, modifiers);
        case SagaUI::UiInputEventType::Text:
            return context_->ProcessTextInput(Rml::String(event.textUtf8));
        case SagaUI::UiInputEventType::MouseMove:
            return context_->ProcessMouseMove(event.x, event.y, modifiers);
        case SagaUI::UiInputEventType::MouseButton:
            return SubmitMouseButton(event, modifiers);
        case SagaUI::UiInputEventType::MouseWheel:
            return context_->ProcessMouseWheel(event.wheelY, modifiers);
        default:
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::UnsupportedInput,
                SagaUI::UiDiagnosticSeverity::Warning,
                "UI input event type is not supported.");
            return false;
        }
    }

    [[nodiscard]] const std::vector<SagaUI::UiDiagnostic>& Diagnostics()
        const noexcept override
    {
        return diagnosticStorage_;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnosticStorage_.clear();
    }

    [[nodiscard]] SagaUI::UiRenderStats LastRenderStats()
        const noexcept override
    {
        return lastRenderStats_;
    }

private:
    class EventBridgeListener final : public Rml::EventListener
    {
    public:
        EventBridgeListener(
            RmlUiBackend& owner,
            SagaUI::UiEventType eventType)
            : owner_(owner),
              eventType_(eventType)
        {
        }

        void ProcessEvent(Rml::Event& event) override
        {
            owner_.EmitUiEvent(event, eventType_);
        }

    private:
        RmlUiBackend& owner_;
        SagaUI::UiEventType eventType_;
    };

    /// Loaded RmlUi document state.
    struct DocumentState
    {
        Rml::ElementDocument* document = nullptr; ///< RmlUi-owned document.
        std::filesystem::path path; ///< Resolved document path.
        SagaUI::UiScreenId screenId;
        SagaUI::UiDocumentId documentId;
        SagaUI::UiDocumentHandle handle =
            SagaUI::UiDocumentHandle::kInvalid;
    };

    void AttachEventListeners()
    {
        clickListener_ = std::make_unique<EventBridgeListener>(
            *this,
            SagaUI::UiEventType::Click);
        submitListener_ = std::make_unique<EventBridgeListener>(
            *this,
            SagaUI::UiEventType::Submit);
        textChangedListener_ = std::make_unique<EventBridgeListener>(
            *this,
            SagaUI::UiEventType::TextChanged);
        focusGainedListener_ = std::make_unique<EventBridgeListener>(
            *this,
            SagaUI::UiEventType::FocusGained);
        focusLostListener_ = std::make_unique<EventBridgeListener>(
            *this,
            SagaUI::UiEventType::FocusLost);

        context_->AddEventListener("click", clickListener_.get());
        context_->AddEventListener("submit", submitListener_.get());
        context_->AddEventListener("change", textChangedListener_.get());
        context_->AddEventListener("focus", focusGainedListener_.get(), true);
        context_->AddEventListener("blur", focusLostListener_.get(), true);
    }

    void RemoveEventListeners()
    {
        if (context_)
        {
            if (clickListener_)
            {
                context_->RemoveEventListener("click", clickListener_.get());
            }
            if (submitListener_)
            {
                context_->RemoveEventListener("submit", submitListener_.get());
            }
            if (textChangedListener_)
            {
                context_->RemoveEventListener(
                    "change",
                    textChangedListener_.get());
            }
            if (focusGainedListener_)
            {
                context_->RemoveEventListener(
                    "focus",
                    focusGainedListener_.get(),
                    true);
            }
            if (focusLostListener_)
            {
                context_->RemoveEventListener(
                    "blur",
                    focusLostListener_.get(),
                    true);
            }
        }

        clickListener_.reset();
        submitListener_.reset();
        textChangedListener_.reset();
        focusGainedListener_.reset();
        focusLostListener_.reset();
    }

    void EmitUiEvent(
        Rml::Event& event,
        SagaUI::UiEventType eventType)
    {
        if (!config_.eventSink)
        {
            return;
        }

        Rml::Element* target = event.GetTargetElement();
        if (!target)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::InputRejected,
                SagaUI::UiDiagnosticSeverity::Warning,
                "RmlUi event target is missing.");
            return;
        }

        const Rml::String& rmlElementId = target->GetId();
        if (rmlElementId.empty())
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::InputRejected,
                SagaUI::UiDiagnosticSeverity::Warning,
                "RmlUi event target has no element id.");
            return;
        }

        DocumentState* document = FindDocumentByElement(*target);
        if (!document)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::InvalidDocumentHandle,
                SagaUI::UiDiagnosticSeverity::Warning,
                "RmlUi event target document is not tracked.");
            return;
        }

        SagaUI::UiEvent uiEvent;
        uiEvent.screenId = document->screenId;
        uiEvent.documentId = document->documentId;
        uiEvent.document = document->handle;
        uiEvent.elementId =
            SagaUI::UiElementId::FromString(std::string(rmlElementId));
        uiEvent.type = eventType;
        uiEvent.x = event.GetParameter<int>("mouse_x", 0);
        uiEvent.y = event.GetParameter<int>("mouse_y", 0);

        if (eventType == SagaUI::UiEventType::TextChanged)
        {
            uiEvent.text = ExtractElementText(*target);
        }

        if (!config_.eventSink->PushEvent(std::move(uiEvent)))
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::InputRejected,
                SagaUI::UiDiagnosticSeverity::Warning,
                "Saga UI event sink rejected a RmlUi event.",
                {},
                document->handle);
        }
    }

    [[nodiscard]] std::string ExtractElementText(Rml::Element& element) const
    {
        if (Rml::ElementFormControl* control =
                dynamic_cast<Rml::ElementFormControl*>(&element))
        {
            return std::string(control->GetValue());
        }

        return std::string(
            element.GetAttribute<Rml::String>("value", Rml::String()));
    }

    [[nodiscard]] DocumentState* FindDocumentByElement(Rml::Element& element)
    {
        Rml::ElementDocument* owner = element.GetOwnerDocument();
        if (!owner)
        {
            return nullptr;
        }

        for (auto& [handle, document] : documents_)
        {
            (void)handle;
            if (document.document == owner)
            {
                return &document;
            }
        }

        return nullptr;
    }

    [[nodiscard]] bool RequireInitialized()
    {
        if (initialized_ && context_)
        {
            return true;
        }

        diagnostics_.Add(
            SagaUI::UiDiagnosticCode::BackendNotInitialized,
            SagaUI::UiDiagnosticSeverity::Error,
            "RmlUi backend is not initialized.");
        return false;
    }

    [[nodiscard]] DocumentState* FindDocument(SagaUI::UiDocumentHandle handle)
    {
        if (!RequireInitialized())
        {
            return nullptr;
        }

        const auto it = documents_.find(handle);
        if (it == documents_.end())
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::InvalidDocumentHandle,
                SagaUI::UiDiagnosticSeverity::Error,
                "UI document handle is not loaded.",
                {},
                handle);
            return nullptr;
        }

        return &it->second;
    }

    [[nodiscard]] std::filesystem::path Resolve(
        const SagaUI::UiDocumentRequest& request)
    {
        if (request.UsesDocumentId())
        {
            if (!config_.resourceProvider)
            {
                diagnostics_.Add(
                    SagaUI::UiDiagnosticCode::DocumentLoadFailed,
                    SagaUI::UiDiagnosticSeverity::Error,
                    "UI document id cannot be resolved without a resource provider.");
                return {};
            }

            const SagaUI::UiResourceResolveResult result =
                config_.resourceProvider->ResolveDocument(request.documentId);
            if (!result.Succeeded())
            {
                diagnostics_.Add(
                    SagaUI::UiDiagnosticCode::DocumentLoadFailed,
                    SagaUI::UiDiagnosticSeverity::Error,
                    result.diagnostic,
                    result.path);
                return {};
            }

            return result.path;
        }

        return ResolvePath(config_.assetRoot, request.path);
    }

    [[nodiscard]] bool SubmitKey(
        const SagaUI::UiInputEvent& event,
        int modifiers)
    {
        const Rml::Input::KeyIdentifier key = ToRmlKey(event.key);
        if (key == Rml::Input::KI_UNKNOWN)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::UnsupportedInput,
                SagaUI::UiDiagnosticSeverity::Warning,
                "UI key input is not mapped to RmlUi.");
            return false;
        }

        if (event.pressed)
        {
            return context_->ProcessKeyDown(key, modifiers);
        }

        return context_->ProcessKeyUp(key, modifiers);
    }

    [[nodiscard]] bool SubmitMouseButton(
        const SagaUI::UiInputEvent& event,
        int modifiers)
    {
        const int button = ToRmlMouseButton(event.mouseButton);
        if (button < 0)
        {
            diagnostics_.Add(
                SagaUI::UiDiagnosticCode::UnsupportedInput,
                SagaUI::UiDiagnosticSeverity::Warning,
                "UI mouse button input is not mapped to RmlUi.");
            return false;
        }

        if (event.pressed)
        {
            return context_->ProcessMouseButtonDown(button, modifiers);
        }

        return context_->ProcessMouseButtonUp(button, modifiers);
    }

    std::vector<SagaUI::UiDiagnostic> diagnosticStorage_;
    DiagnosticCollector diagnostics_{diagnosticStorage_};
    SagaUI::UiBackendConfig config_;
    std::unique_ptr<RmlUiFileInterface> fileInterface_;
    std::unique_ptr<RmlUiSystemInterface> systemInterface_;
    std::unique_ptr<EventBridgeListener> clickListener_;
    std::unique_ptr<EventBridgeListener> submitListener_;
    std::unique_ptr<EventBridgeListener> textChangedListener_;
    std::unique_ptr<EventBridgeListener> focusGainedListener_;
    std::unique_ptr<EventBridgeListener> focusLostListener_;
    RmlUiCountingRenderInterface renderInterface_;
    Rml::Context* context_ = nullptr;
    std::unordered_map<SagaUI::UiDocumentHandle, DocumentState> documents_;
    SagaUI::UiRenderStats lastRenderStats_;
    std::uint32_t nextHandle_ = 1;
    bool initialized_ = false;
};

} // namespace

std::unique_ptr<SagaUI::IUiBackend> CreateRmlUiUiBackend()
{
    return std::make_unique<RmlUiBackend>();
}

} // namespace SagaEngine::UI::Backends
