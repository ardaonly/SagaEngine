/// @file UiNamedActionHandlerTests.cpp
/// @brief Unit tests for backend-neutral UI named action handlers.

#include <gtest/gtest.h>

#include "SagaEngine/UI/CSharpUiNamedActionHandler.h"
#include "SagaEngine/UI/IUiNamedActionHandler.h"
#include "SagaEngine/UI/RuntimeUiBootstrapper.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

namespace SagaUI = ::SagaEngine::UI;
namespace SagaScript = ::SagaEngine::Scripting;

class RecordingNamedActionHandler final
    : public SagaUI::IUiNamedActionHandler
{
public:
    [[nodiscard]] SagaUI::UiNamedActionHandlerResult Invoke(
        const SagaUI::UiNamedActionContext& context) override
    {
        contexts.push_back(context);
        SagaUI::UiNamedActionHandlerResult result = nextResult;
        if (!result.context.actionId.IsSet())
        {
            result.context = context;
        }
        return result;
    }

    std::vector<SagaUI::UiNamedActionContext> contexts;
    SagaUI::UiNamedActionHandlerResult nextResult;
};

class RecordingScriptHost final : public SagaScript::ISagaScriptHost
{
public:
    [[nodiscard]] SagaScript::ScriptPackageLoadResult LoadPackage(
        const SagaScript::ScriptPackageLoadRequest&) override
    {
        return {};
    }

    [[nodiscard]] SagaScript::ScriptInstanceCreateResult CreateInstance(
        SagaScript::ScriptPackageHandle,
        const SagaScript::ScriptClassId&) override
    {
        return {};
    }

    [[nodiscard]] SagaScript::ScriptHostOperationResult InvokeLifecycle(
        const SagaScript::ScriptLifecycleInvocation&) override
    {
        return {};
    }

    [[nodiscard]] SagaScript::ScriptHostOperationResult InvokeUiNamedAction(
        const SagaScript::ScriptUiNamedActionInvocation& invocation) override
    {
        invocations.push_back(invocation);
        return nextResult;
    }

    std::vector<SagaScript::ScriptUiNamedActionInvocation> invocations;
    SagaScript::ScriptHostOperationResult nextResult{true, {}};
};

[[nodiscard]] SagaUI::UiNamedAction MakeNamedAction(
    const char* actionId = "connect_to_server")
{
    SagaUI::UiNamedAction action;
    action.actionId = SagaUI::UiActionId::FromString(actionId);
    action.sourceEvent.sequence = 42;
    action.sourceEvent.screenId =
        SagaUI::UiScreenId::FromString("connect_screen");
    action.sourceEvent.documentId =
        SagaUI::UiDocumentId::FromContent("connect_screen");
    action.sourceEvent.document =
        static_cast<SagaUI::UiDocumentHandle>(7);
    action.sourceEvent.elementId =
        SagaUI::UiElementId::FromString("connect_button");
    action.sourceEvent.type = SagaUI::UiEventType::Click;
    action.sourceEvent.text = "local-dev";
    return action;
}

[[nodiscard]] SagaUI::UiActionBinding MakeConnectBinding()
{
    SagaUI::UiActionBinding binding;
    binding.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    binding.elementId = SagaUI::UiElementId::FromString("connect_button");
    binding.eventType = SagaUI::UiEventType::Click;
    binding.actionId = SagaUI::UiActionId::FromString("connect_to_server");
    binding.actionType = SagaUI::UiActionType::EmitNamedAction;
    return binding;
}

[[nodiscard]] SagaUI::UiEvent MakeConnectClickEvent()
{
    SagaUI::UiEvent event;
    event.screenId = SagaUI::UiScreenId::FromString("connect_screen");
    event.documentId = SagaUI::UiDocumentId::FromContent("connect_screen");
    event.elementId = SagaUI::UiElementId::FromString("connect_button");
    event.type = SagaUI::UiEventType::Click;
    event.text = "local-dev";
    return event;
}

[[nodiscard]] std::filesystem::path TestRoot(const char* name)
{
    return std::filesystem::temp_directory_path() /
           "SagaUiNamedActionHandlerTests" / name;
}

void WriteConnectScreen(const std::filesystem::path& root)
{
    std::filesystem::create_directories(root / "UI");
    std::ofstream document(root / "UI" / "connect_screen.rml");
    document << "<rml/>";
}

[[nodiscard]] SagaUI::UiRuntimeBootstrapRequest MakeRuntimeRequest(
    std::filesystem::path contentRoot)
{
    SagaUI::UiRuntimeBootstrapRequest request;
    request.backend = std::make_unique<SagaUI::NullUiBackend>();
    request.viewport = SagaUI::UiViewport{640, 360, 1.0f};
    request.contextName = "UiNamedActionHandlerTests";
    request.contentRoot = std::move(contentRoot);
    return request;
}

} // namespace

TEST(UiNamedActionHandlerTests, RegisterHandlerSucceeds)
{
    std::unique_ptr<SagaUI::IUiNamedActionHandlerRegistry> registry =
        SagaUI::CreateUiNamedActionHandlerRegistry();

    const SagaUI::UiNamedActionHandlerResult result =
        registry->RegisterHandler(
            SagaUI::UiActionId::FromString("connect_to_server"),
            std::make_shared<RecordingNamedActionHandler>());

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(
        result.code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::None);
    EXPECT_TRUE(registry->Diagnostics().empty());
}

TEST(UiNamedActionHandlerTests, DuplicateHandlerReturnsDiagnostic)
{
    std::unique_ptr<SagaUI::IUiNamedActionHandlerRegistry> registry =
        SagaUI::CreateUiNamedActionHandlerRegistry();
    auto handler = std::make_shared<RecordingNamedActionHandler>();

    ASSERT_TRUE(registry->RegisterHandler(
        SagaUI::UiActionId::FromString("connect_to_server"),
        handler).Succeeded());

    const SagaUI::UiNamedActionHandlerResult duplicate =
        registry->RegisterHandler(
            SagaUI::UiActionId::FromString("connect_to_server"),
            handler);

    EXPECT_EQ(
        duplicate.code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::DuplicateHandler);
    ASSERT_EQ(registry->Diagnostics().size(), 1U);
    EXPECT_EQ(
        registry->Diagnostics().front().code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::DuplicateHandler);
}

TEST(UiNamedActionHandlerTests, MissingHandlerReturnsDeterministicDiagnostic)
{
    std::unique_ptr<SagaUI::IUiNamedActionHandlerRegistry> registry =
        SagaUI::CreateUiNamedActionHandlerRegistry();

    const SagaUI::UiNamedActionHandlerResult result =
        registry->DispatchNamedAction(MakeNamedAction());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(
        result.code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::MissingHandler);
    ASSERT_EQ(registry->Diagnostics().size(), 1U);
    EXPECT_EQ(registry->Diagnostics().front().context.actionId.value,
              "connect_to_server");
}

TEST(UiNamedActionHandlerTests, ContextPreservesNamedActionMetadata)
{
    std::unique_ptr<SagaUI::IUiNamedActionHandlerRegistry> registry =
        SagaUI::CreateUiNamedActionHandlerRegistry();
    auto handler = std::make_shared<RecordingNamedActionHandler>();
    ASSERT_TRUE(registry->RegisterHandler(
        SagaUI::UiActionId::FromString("connect_to_server"),
        handler).Succeeded());

    const SagaUI::UiNamedActionHandlerResult result =
        registry->DispatchNamedAction(MakeNamedAction());

    EXPECT_TRUE(result.Succeeded());
    ASSERT_EQ(handler->contexts.size(), 1U);
    const SagaUI::UiNamedActionContext& context = handler->contexts.front();
    EXPECT_EQ(context.actionId.value, "connect_to_server");
    EXPECT_EQ(context.screenId.value, "connect_screen");
    EXPECT_EQ(context.documentId.name, "connect_screen");
    EXPECT_EQ(context.document, static_cast<SagaUI::UiDocumentHandle>(7));
    EXPECT_EQ(context.elementId.value, "connect_button");
    EXPECT_EQ(context.eventType, SagaUI::UiEventType::Click);
    EXPECT_EQ(context.text, "local-dev");
    EXPECT_EQ(context.sourceEvent.sequence, 42U);
}

TEST(UiNamedActionHandlerTests, HandlerFailureReturnsDiagnostic)
{
    std::unique_ptr<SagaUI::IUiNamedActionHandlerRegistry> registry =
        SagaUI::CreateUiNamedActionHandlerRegistry();
    auto handler = std::make_shared<RecordingNamedActionHandler>();
    handler->nextResult.code =
        SagaUI::UiNamedActionHandlerDiagnosticCode::HandlerFailed;
    handler->nextResult.severity = SagaUI::UiDiagnosticSeverity::Error;
    handler->nextResult.message = "connect failed";

    ASSERT_TRUE(registry->RegisterHandler(
        SagaUI::UiActionId::FromString("connect_to_server"),
        handler).Succeeded());

    const SagaUI::UiNamedActionHandlerResult result =
        registry->DispatchNamedAction(MakeNamedAction());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(
        result.code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::HandlerFailed);
    EXPECT_EQ(result.message, "connect failed");
    ASSERT_EQ(registry->Diagnostics().size(), 1U);
    EXPECT_EQ(
        registry->Diagnostics().front().code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::HandlerFailed);
}

TEST(UiNamedActionHandlerTests, RuntimeDispatchNamedActionsDrainsOnce)
{
    const std::filesystem::path root = TestRoot("RuntimeDispatch");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    auto handler = std::make_shared<RecordingNamedActionHandler>();
    ASSERT_TRUE(result.context->RegisterActionBinding(
        MakeConnectBinding()).Succeeded());
    ASSERT_TRUE(result.context->RegisterNamedActionHandler(
        SagaUI::UiActionId::FromString("connect_to_server"),
        handler).Succeeded());

    ASSERT_TRUE(result.context->QueueUiEventForTesting(
        MakeConnectClickEvent()));
    const std::vector<SagaUI::UiActionDispatchResult> actionResults =
        result.context->DispatchUiEvents();
    ASSERT_EQ(actionResults.size(), 1U);
    EXPECT_EQ(
        actionResults.front().code,
        SagaUI::UiActionDiagnosticCode::NamedActionEmitted);

    const std::vector<SagaUI::UiNamedActionHandlerResult> handlerResults =
        result.context->DispatchNamedActions();
    ASSERT_EQ(handlerResults.size(), 1U);
    EXPECT_TRUE(handlerResults.front().Succeeded());
    ASSERT_EQ(handler->contexts.size(), 1U);
    EXPECT_EQ(handler->contexts.front().text, "local-dev");

    EXPECT_TRUE(result.context->DispatchNamedActions().empty());
    EXPECT_TRUE(result.context->DrainNamedActions().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiNamedActionHandlerTests, RuntimeDispatchNamedActionsInvokesCSharpAdapter)
{
    const std::filesystem::path root = TestRoot("RuntimeCSharpAdapterDispatch");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    RecordingScriptHost scriptHost;
    auto handler = std::make_shared<SagaUI::CSharpUiNamedActionHandler>(
        SagaUI::CSharpUiNamedActionHandlerOptions{
            scriptHost,
            SagaScript::ScriptInstanceHandle{99},
            "OnConnect",
        });

    ASSERT_TRUE(result.context->RegisterActionBinding(
        MakeConnectBinding()).Succeeded());
    ASSERT_TRUE(result.context->RegisterNamedActionHandler(
        SagaUI::UiActionId::FromString("connect_to_server"),
        handler).Succeeded());

    ASSERT_TRUE(result.context->QueueUiEventForTesting(
        MakeConnectClickEvent()));
    ASSERT_EQ(result.context->DispatchUiEvents().size(), 1U);

    const std::vector<SagaUI::UiNamedActionHandlerResult> handlerResults =
        result.context->DispatchNamedActions();

    ASSERT_EQ(handlerResults.size(), 1U);
    EXPECT_TRUE(handlerResults.front().Succeeded());
    ASSERT_EQ(scriptHost.invocations.size(), 1U);
    EXPECT_EQ(scriptHost.invocations.front().instance.value, 99U);
    EXPECT_EQ(scriptHost.invocations.front().methodName, "OnConnect");
    EXPECT_EQ(
        scriptHost.invocations.front().context.actionId,
        "connect_to_server");
    EXPECT_EQ(scriptHost.invocations.front().context.screenId, "connect_screen");
    EXPECT_EQ(
        scriptHost.invocations.front().context.elementId,
        "connect_button");
    EXPECT_EQ(
        scriptHost.invocations.front().context.eventType,
        SagaScript::ScriptUiNamedActionEventType::Click);
    EXPECT_EQ(scriptHost.invocations.front().context.text, "local-dev");

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiNamedActionHandlerTests, RuntimeMissingHandlerProducesDiagnostic)
{
    const std::filesystem::path root = TestRoot("RuntimeMissingHandler");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    ASSERT_TRUE(result.context->RegisterActionBinding(
        MakeConnectBinding()).Succeeded());
    ASSERT_TRUE(result.context->QueueUiEventForTesting(
        MakeConnectClickEvent()));
    ASSERT_EQ(result.context->DispatchUiEvents().size(), 1U);

    const std::vector<SagaUI::UiNamedActionHandlerResult> handlerResults =
        result.context->DispatchNamedActions();
    ASSERT_EQ(handlerResults.size(), 1U);
    EXPECT_EQ(
        handlerResults.front().code,
        SagaUI::UiNamedActionHandlerDiagnosticCode::MissingHandler);
    ASSERT_EQ(result.context->NamedActionHandlerDiagnostics().size(), 1U);

    result.context->ClearNamedActionHandlerDiagnostics();
    EXPECT_TRUE(result.context->NamedActionHandlerDiagnostics().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}

TEST(UiNamedActionHandlerTests, RuntimeStartsWithEmptyNamedActionDispatch)
{
    const std::filesystem::path root = TestRoot("RuntimeEmpty");
    std::filesystem::remove_all(root);
    WriteConnectScreen(root);

    SagaUI::UiRuntimeBootstrapResult result =
        SagaUI::BootstrapRuntimeUi(MakeRuntimeRequest(root));
    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.context, nullptr);

    EXPECT_TRUE(result.context->DispatchNamedActions().empty());
    EXPECT_TRUE(result.context->NamedActionHandlerDiagnostics().empty());

    result.context->Shutdown();
    std::filesystem::remove_all(root);
}
