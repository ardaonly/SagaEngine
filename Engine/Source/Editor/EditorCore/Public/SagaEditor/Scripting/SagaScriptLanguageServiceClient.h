// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Processes/ManagedToolRuntime.h"

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

struct LanguageServicePosition { std::uint32_t line = 0; std::uint32_t character = 0; };
struct LanguageServiceRange { LanguageServicePosition start; LanguageServicePosition end; };
struct LanguageServiceDiagnostic
{
    std::string id;
    std::string severity;
    std::string message;
    LanguageServiceRange range;
};
struct LanguageServiceClassification { std::string kind; LanguageServiceRange range; };
struct LanguageServiceCompletion { std::string label; std::string filterText; std::string sortText; std::vector<std::string> tags; };
struct LanguageServiceLocation { std::filesystem::path path; LanguageServiceRange range; };
struct LanguageServiceSymbol { std::string name; std::string kind; LanguageServiceRange range; };

struct LanguageServiceDocumentIdentity
{
    std::string documentId;
    std::filesystem::path canonicalPath;
    std::uint64_t version = 0;
};

struct LanguageServiceMetadataReference
{
    std::filesystem::path canonicalPath;
    std::string hash;
};

struct LanguageServiceProjectSnapshot
{
    std::string projectSessionId;
    std::string projectSnapshotIdentity;
    std::filesystem::path canonicalRoot;
    std::string projectId;
    std::string manifestHash;
    std::string lockHash;
    std::string packageGeneration;
    std::string referenceManifestHash;
    std::string ownershipManifestHash;
    std::string runtimeBridgeIdentity;
    std::vector<LanguageServiceMetadataReference> metadataReferences;
    std::string compilerVersion = "4.11.0";
    std::string languageVersion = "latest-major";
};

struct LanguageServiceResponse
{
    std::uint64_t requestId = 0;
    std::string projectSessionId;
    std::string projectSnapshotIdentity;
    std::optional<std::string> documentId;
    std::optional<std::uint64_t> documentVersion;
    std::string method;
    std::string payloadJson;
    std::vector<LanguageServiceDiagnostic> diagnostics;
    std::vector<LanguageServiceClassification> classifications;
    std::vector<LanguageServiceCompletion> completions;
    std::vector<LanguageServiceLocation> locations;
    std::vector<LanguageServiceSymbol> symbols;
    std::string hoverText;
    std::string errorCode;
    std::string errorMessage;
    [[nodiscard]] bool Succeeded() const noexcept { return errorCode.empty(); }
};

enum class LanguageServiceState { Stopped, Starting, Ready, Recovering, Unavailable };

class SagaScriptLanguageServiceClient final
{
public:
    using PublicationCallback = std::function<void(LanguageServiceResponse)>;
    using StateCallback = std::function<void(LanguageServiceState)>;

    explicit SagaScriptLanguageServiceClient(ManagedToolRuntimeOptions runtime = {});
    ~SagaScriptLanguageServiceClient();
    SagaScriptLanguageServiceClient(const SagaScriptLanguageServiceClient&) = delete;
    SagaScriptLanguageServiceClient& operator=(const SagaScriptLanguageServiceClient&) = delete;

    [[nodiscard]] bool Start(const LanguageServiceProjectSnapshot& snapshot);
    void Stop();
    [[nodiscard]] std::future<LanguageServiceResponse> OpenDocument(
        const LanguageServiceDocumentIdentity& document, std::string fullText);
    [[nodiscard]] std::future<LanguageServiceResponse> ChangeDocument(
        const LanguageServiceDocumentIdentity& document, std::string fullText);
    [[nodiscard]] std::future<LanguageServiceResponse> SaveDocument(
        const LanguageServiceDocumentIdentity& document, std::string fullText);
    [[nodiscard]] std::future<LanguageServiceResponse> CloseDocument(
        const LanguageServiceDocumentIdentity& document);
    [[nodiscard]] std::future<LanguageServiceResponse> Request(
        std::string method, const LanguageServiceDocumentIdentity& document,
        std::optional<LanguageServicePosition> position = std::nullopt,
        std::string extraPayloadJson = "{}");
    [[nodiscard]] std::future<LanguageServiceResponse> Preflight(
        const LanguageServiceDocumentIdentity& document, std::string candidateText);
    void Cancel(std::uint64_t requestId);
    [[nodiscard]] bool Reconfigure(LanguageServiceProjectSnapshot snapshot);
    void SetPublicationCallback(PublicationCallback callback);
    void SetStateCallback(StateCallback callback);
    [[nodiscard]] LanguageServiceState State() const noexcept;
    [[nodiscard]] std::uint64_t ProcessId() const noexcept;
    [[nodiscard]] std::string LastDiagnostic() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SagaEditor
