// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostSupport.cpp

#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <sstream>
#include <utility>

#include "CSharpScriptHostImpl.hpp"
#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

namespace SagaEngine::Scripting
{

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(std::string code,
                                              std::string title,
                                              std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity = SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category = SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source = SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

[[nodiscard]] std::filesystem::path NormalizeRoot(const std::filesystem::path& root)
{
    return std::filesystem::absolute(root).lexically_normal();
}

[[nodiscard]] std::filesystem::path ResolvePackagePath(const std::filesystem::path& packageRoot,
                                                       const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (packageRoot / path).lexically_normal();
}

[[nodiscard]] bool HasStringField(const nlohmann::json& object, std::string_view field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_string();
}

[[nodiscard]] std::string ReadStringField(const nlohmann::json& object, std::string_view field)
{
    return object.at(std::string(field)).get<std::string>();
}

[[nodiscard]] std::vector<std::string> ReadStringArrayField(const nlohmann::json& object,
                                                            std::string_view field)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_array())
    {
        return {};
    }

    std::vector<std::string> values;
    for (const auto& value : *iterator)
    {
        if (value.is_string())
        {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

#if defined(_WIN32)
[[nodiscard]] std::wstring ToHostString(const std::filesystem::path& path)
{
    return path.wstring();
}

[[nodiscard]] std::wstring ToHostString(std::string_view value)
{
    return std::wstring(value.begin(), value.end());
}
#else
[[nodiscard]] std::string ToHostString(const std::filesystem::path& path)
{
    return path.string();
}

[[nodiscard]] std::string ToHostString(std::string_view value)
{
    return std::string(value);
}
#endif

[[nodiscard]] std::filesystem::path RuntimeBridgeFromEnvironment()
{
    const char* value = std::getenv("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY");
    if (value == nullptr || std::string_view(value).empty())
    {
        return {};
    }

    return std::filesystem::path(value);
}

[[nodiscard]] std::filesystem::path DotnetRootFromEnvironment()
{
    const char* value = std::getenv("DOTNET_ROOT");
    if (value == nullptr || std::string_view(value).empty())
    {
        return {};
    }

    return std::filesystem::path(value);
}

[[nodiscard]] std::string Sha256File(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {};
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context || EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1)
    {
        if (context)
            EVP_MD_CTX_free(context);
        return {};
    }
    std::array<char, 16384> buffer{};
    while (input)
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 &&
            EVP_DigestUpdate(context, buffer.data(), static_cast<std::size_t>(count)) != 1)
        {
            EVP_MD_CTX_free(context);
            return {};
        }
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context, digest.data(), &length) != 1)
    {
        EVP_MD_CTX_free(context);
        return {};
    }
    EVP_MD_CTX_free(context);
    std::ostringstream output;
    output << "sha256:" << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < length; ++index)
        output << std::setw(2) << static_cast<unsigned int>(digest[index]);
    return output.str();
}

[[nodiscard]] std::string BridgeError(const std::array<char, kBridgeErrorBufferSize>& buffer)
{
    return std::string(buffer.data());
}

[[nodiscard]] std::string CodeForBridgeStatus(BridgeStatus status, std::string_view operation)
{
    switch (status)
    {
    case BridgeStatus::ClassMissing:
        return ScriptHostDiagnostics::ClassMissing;
    case BridgeStatus::ClassInvalid:
        return ScriptHostDiagnostics::ClassInvalid;
    case BridgeStatus::InstanceCreationFailed:
        return ScriptHostDiagnostics::InstanceCreationFailed;
    case BridgeStatus::LifecycleMethodMissing:
        return ScriptHostDiagnostics::LifecycleMethodMissing;
    case BridgeStatus::LifecycleFailed:
        return ScriptHostDiagnostics::LifecycleFailed;
    case BridgeStatus::ManagedException:
        return ScriptHostDiagnostics::ManagedException;
    case BridgeStatus::UiNamedActionMethodMissing:
        return ScriptHostDiagnostics::UiNamedActionMethodMissing;
    case BridgeStatus::UiNamedActionInvalidSignature:
        return ScriptHostDiagnostics::UiNamedActionInvalidSignature;
    case BridgeStatus::UiNamedActionReturnedFalse:
        return ScriptHostDiagnostics::UiNamedActionReturnedFalse;
    case BridgeStatus::NamedActionMethodMissing:
        return ScriptHostDiagnostics::NamedActionMethodMissing;
    case BridgeStatus::NamedActionInvalidContract:
        return ScriptHostDiagnostics::NamedActionInvalidContract;
    case BridgeStatus::Int32BinaryMethodMissing:
        return ScriptHostDiagnostics::Int32BinaryMethodMissing;
    case BridgeStatus::Int32BinaryInvalidSignature:
        return ScriptHostDiagnostics::Int32BinaryMethodInvalidSignature;
    case BridgeStatus::AssemblyLoadFailed:
        return ScriptHostDiagnostics::AssemblyLoadFailed;
    case BridgeStatus::InvalidArgument:
        return operation == "create"
                   ? ScriptHostDiagnostics::InvalidPackageHandle
                   : (operation == "uiNamedAction" || operation == "namedAction" ||
                              operation == "int32Binary"
                          ? ScriptHostDiagnostics::InvalidInstanceHandle
                          : ScriptHostDiagnostics::LifecycleFailed);
    case BridgeStatus::Ok:
        break;
    }

    return ScriptHostDiagnostics::LifecycleFailed;
}

[[nodiscard]] std::string TitleForDiagnostic(std::string_view code)
{
    if (code == ScriptHostDiagnostics::ClassMissing)
    {
        return "Script class not found";
    }
    if (code == ScriptHostDiagnostics::ClassInvalid)
    {
        return "Invalid script class";
    }
    if (code == ScriptHostDiagnostics::InstanceCreationFailed)
    {
        return "Script instance creation failed";
    }
    if (code == ScriptHostDiagnostics::ManagedException)
    {
        return "Managed script exception";
    }
    if (code == ScriptHostDiagnostics::AssemblyLoadFailed)
    {
        return "Managed script assembly load failed";
    }
    if (code == ScriptHostDiagnostics::ScriptCapabilityDenied)
    {
        return "Script capability denied";
    }
    if (code == ScriptHostDiagnostics::InvalidEntityHandle)
    {
        return "Invalid script entity handle";
    }
    if (code == ScriptHostDiagnostics::ScriptWorldUnavailable)
    {
        return "Script world unavailable";
    }
    if (code == ScriptHostDiagnostics::UiNamedActionMethodMissing)
    {
        return "UI named action method not found";
    }
    if (code == ScriptHostDiagnostics::UiNamedActionInvalidSignature)
    {
        return "Invalid UI named action signature";
    }
    if (code == ScriptHostDiagnostics::UiNamedActionReturnedFalse)
    {
        return "UI named action rejected";
    }
    if (code == ScriptHostDiagnostics::NamedActionMethodMissing)
    {
        return "Named action method not found";
    }
    if (code == ScriptHostDiagnostics::NamedActionInvalidContract)
    {
        return "Invalid named action contract";
    }
    if (code == ScriptHostDiagnostics::Int32BinaryMethodMissing)
    {
        return "Int32 binary method not found";
    }
    if (code == ScriptHostDiagnostics::Int32BinaryMethodInvalidSignature)
    {
        return "Invalid Int32 binary method signature";
    }

    return "C# script host failure";
}

} // namespace SagaEngine::Scripting
