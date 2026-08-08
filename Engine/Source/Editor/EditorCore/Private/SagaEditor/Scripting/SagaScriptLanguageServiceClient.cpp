// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Scripting/SagaScriptLanguageServiceClient.h"

#include "SagaEditor/Processes/ToolProcessSession.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

namespace SagaEditor
{
using Json = nlohmann::json;
namespace
{
LanguageServiceRange ParseRange(const Json& value)
{
    return {{value.at("start").at("line").get<std::uint32_t>(),
             value.at("start").at("character").get<std::uint32_t>()},
            {value.at("end").at("line").get<std::uint32_t>(),
             value.at("end").at("character").get<std::uint32_t>()}};
}

void ParseDiagnostics(const Json& values, LanguageServiceResponse& response)
{
    if (!values.is_array()) return;
    for (const auto& value : values)
        response.diagnostics.push_back({value.value("id", ""), value.value("severity", ""),
            value.value("message", ""), ParseRange(value.at("range"))});
}

void ParseTypedPayload(const Json& payload, LanguageServiceResponse& response)
{
    if (payload.contains("diagnostics")) ParseDiagnostics(payload["diagnostics"], response);
    else if (response.method == "diagnostics") ParseDiagnostics(payload, response);
    const auto* classifications = payload.is_object() && payload.contains("classifications") ?
        &payload["classifications"] : response.method == "classifications" ? &payload : nullptr;
    if (classifications && classifications->is_array())
        for (const auto& value : *classifications)
            response.classifications.push_back({value.value("kind", ""), ParseRange(value.at("range"))});
    if (response.method == "completion" && payload.is_array())
        for (const auto& value : payload)
        {
            LanguageServiceCompletion item;
            item.label = value.value("label", ""); item.filterText = value.value("filterText", "");
            item.sortText = value.value("sortText", "");
            for (const auto& tag : value.value("tags", Json::array()))
                item.tags.push_back(tag.get<std::string>());
            response.completions.push_back(std::move(item));
        }
    if (response.method == "hover" && payload.is_object())
        response.hoverText = payload.value("text", "");
    if ((response.method == "definition" || response.method == "references") && payload.is_array())
        for (const auto& value : payload)
            response.locations.push_back({value.value("path", ""), ParseRange(value.at("range"))});
    if (response.method == "symbols" && payload.is_array())
        for (const auto& value : payload)
            response.symbols.push_back({value.value("name", ""), value.value("kind", ""),
                ParseRange(value.at("range"))});
}
}

class SagaScriptLanguageServiceClient::Impl
{
public:
    struct ReplayDocument
    {
        LanguageServiceDocumentIdentity identity;
        std::string text;
    };

    explicit Impl(ManagedToolRuntimeOptions options) : runtime_(std::move(options)) {}
    ~Impl() { Stop(); }

    bool Start(const LanguageServiceProjectSnapshot& snapshot)
    {
        Stop();
        {
            std::scoped_lock lock(mutex_);
            snapshot_ = snapshot;
            replayDocuments_.clear();
        }
        manualStop_ = false;
        SetState(LanguageServiceState::Starting);
        std::scoped_lock lifecycleLock(lifecycleMutex_);
        if (!LaunchTransport()) return false;
        monitorRunning_ = true;
        monitor_ = std::thread([this] { MonitorLoop(); });
        return true;
    }

    bool LaunchTransport()
    {
        LanguageServiceProjectSnapshot snapshot;
        { std::scoped_lock lock(mutex_); snapshot = snapshot_; }
        ToolProcessRequest process; process.workingDirectory = snapshot.canonicalRoot;
        const auto invocation = runtime_.PrepareInvocation(ManagedToolId::Script,
            {"language-service", "--stdio"}, std::move(process));
        if (!invocation.Ready())
        {
            diagnostic_ = invocation.diagnostics.empty() ? "Language service preflight failed." : invocation.diagnostics.front().message;
            SetState(LanguageServiceState::Unavailable); return false;
        }
        ToolProcessSessionOptions options; options.process = *invocation.process;
        if (!process_.Start(options))
        { diagnostic_ = process_.Diagnostic(); SetState(LanguageServiceState::Unavailable); return false; }
        running_ = true; reader_ = std::thread([this] { ReaderLoop(); });
        const auto initialized = AwaitStartup(Send("initialize", Json::object(), std::nullopt),
            "LanguageService.InitializeTimeout");
        if (!initialized || !initialized->Succeeded())
        {
            diagnostic_ = initialized ? initialized->errorMessage : "Language service initialize timed out.";
            StopTransport(); SetState(LanguageServiceState::Unavailable); return false;
        }
        Json payload = SnapshotJson(snapshot);
        const auto opened = AwaitStartup(Send("project/open", std::move(payload), std::nullopt),
            "LanguageService.ProjectOpenTimeout");
        if (!opened || !opened->Succeeded())
        {
            diagnostic_ = opened ? opened->errorMessage : "Language service project open timed out.";
            StopTransport(); SetState(LanguageServiceState::Unavailable); return false;
        }
        std::map<std::string, ReplayDocument> replay;
        { std::scoped_lock lock(mutex_); replay = replayDocuments_; }
        for (const auto& [id, value] : replay)
        {
            (void)id;
            const auto restored = AwaitStartup(
                Document("document/open", value.identity, value.text),
                "LanguageService.ReplayTimeout");
            if (!restored || !restored->Succeeded())
            {
                diagnostic_ = restored ? restored->errorMessage : "Language service document replay timed out.";
                StopTransport(); SetState(LanguageServiceState::Unavailable); return false;
            }
        }
        SetState(LanguageServiceState::Ready); return true;
    }

    void Stop()
    {
        manualStop_ = true;
        monitorRunning_ = false;
        if (monitor_.joinable() && monitor_.get_id() != std::this_thread::get_id()) monitor_.join();
        std::scoped_lock lifecycleLock(lifecycleMutex_);
        StopTransport();
        SetState(LanguageServiceState::Stopped);
    }

    void StopTransport()
    {
        if (running_)
        {
            try { (void)Send("shutdown", Json::object(), std::nullopt).wait_for(std::chrono::seconds(1)); } catch (...) {}
        }
        running_ = false; process_.Shutdown();
        if (reader_.joinable()) reader_.join();
        std::map<std::uint64_t, std::promise<LanguageServiceResponse>> pending;
        { std::scoped_lock lock(mutex_); pending.swap(pending_); }
        for (auto& [id, promise] : pending)
        {
            LanguageServiceResponse response; response.requestId = id; response.errorCode = "LanguageService.Stopped"; response.errorMessage = "Language service stopped.";
            promise.set_value(std::move(response));
        }
    }

    std::future<LanguageServiceResponse> Document(std::string method,
        const LanguageServiceDocumentIdentity& document, std::string text)
    {
        {
            std::scoped_lock lock(mutex_);
            if (method == "document/close") replayDocuments_.erase(document.documentId);
            else replayDocuments_[document.documentId] = ReplayDocument{document, text};
        }
        Json payload{{"path", document.canonicalPath.string()}};
        if (method != "document/close") payload["text"] = std::move(text);
        return Send(std::move(method), std::move(payload), document);
    }

    std::future<LanguageServiceResponse> Request(std::string method,
        const LanguageServiceDocumentIdentity& document,
        std::optional<LanguageServicePosition> position, std::string extra)
    {
        Json payload;
        try { payload = Json::parse(extra); } catch (...) { payload = Json::object(); }
        if (position) payload["position"] = {{"line", position->line}, {"character", position->character}};
        return Send(std::move(method), std::move(payload), document);
    }

    bool Reconfigure(LanguageServiceProjectSnapshot snapshot)
    {
        std::scoped_lock sendLock(sendMutex_);
        LanguageServiceProjectSnapshot current;
        { std::scoped_lock lock(mutex_); current = snapshot_; }
        if (snapshot.projectSessionId != current.projectSessionId ||
            snapshot.projectId != current.projectId ||
            snapshot.canonicalRoot != current.canonicalRoot ||
            snapshot.projectSnapshotIdentity.empty())
        {
            std::scoped_lock lock(mutex_);
            diagnostic_ = "Language service reconfiguration identity is invalid.";
            return false;
        }
        Json payload = SnapshotJson(snapshot);
        payload["candidateProjectSnapshotIdentity"] =
            snapshot.projectSnapshotIdentity;
        auto future = SendUnlocked("project/reconfigure", std::move(payload),
            std::nullopt, current);
        if (future.wait_for(std::chrono::seconds(15)) != std::future_status::ready)
        {
            {
                std::scoped_lock lock(mutex_);
                diagnostic_ = "Language service project reconfiguration timed out.";
            }
            FailPending("LanguageService.ReconfigureTimeout", diagnostic_);
            SetState(monitorRunning_ && !manualStop_ ?
                LanguageServiceState::Recovering : LanguageServiceState::Unavailable);
            return false;
        }
        const auto response = future.get();
        if (!response.Succeeded())
        {
            std::scoped_lock lock(mutex_);
            diagnostic_ = response.errorCode + ": " + response.errorMessage;
            return false;
        }
        Json responsePayload;
        try { responsePayload = Json::parse(response.payloadJson); }
        catch (...) { responsePayload = Json::object(); }
        if (response.projectSnapshotIdentity != current.projectSnapshotIdentity ||
            responsePayload.value("acceptedProjectSnapshotIdentity", "") !=
                snapshot.projectSnapshotIdentity)
        {
            std::scoped_lock lock(mutex_);
            diagnostic_ = "Language service accepted an unexpected project snapshot identity.";
            return false;
        }
        {
            std::scoped_lock lock(mutex_);
            snapshot_ = std::move(snapshot);
            diagnostic_.clear();
        }
        FailPending("LanguageService.StaleProject",
            "Project snapshot was replaced by a successful reconfiguration.");
        return true;
    }

    void Cancel(std::uint64_t id) { (void)Send("cancel", Json{{"requestId", id}}, std::nullopt); }
    void SetPublicationCallback(PublicationCallback value) { std::scoped_lock lock(mutex_); publication_ = std::move(value); }
    void SetStateCallback(StateCallback value) { std::scoped_lock lock(mutex_); stateCallback_ = std::move(value); }
    LanguageServiceState State() const noexcept { return state_.load(); }
    std::uint64_t ProcessId() const noexcept { return process_.ProcessId(); }
    std::string Diagnostic() const { std::scoped_lock lock(mutex_); return diagnostic_; }

private:
    std::future<LanguageServiceResponse> Send(std::string method, Json payload,
                                              std::optional<LanguageServiceDocumentIdentity> document)
    {
        std::scoped_lock sendLock(sendMutex_);
        LanguageServiceProjectSnapshot snapshot;
        { std::scoped_lock lock(mutex_); snapshot = snapshot_; }
        return SendUnlocked(std::move(method), std::move(payload), document,
            snapshot);
    }

    std::future<LanguageServiceResponse> SendUnlocked(std::string method,
        Json payload,
        const std::optional<LanguageServiceDocumentIdentity>& document,
        const LanguageServiceProjectSnapshot& snapshot)
    {
        const auto id = nextRequest_.fetch_add(1);
        Json envelope{{"schemaVersion", 1}, {"protocolVersion", 1}, {"messageType", "request"},
            {"requestId", id}, {"projectSessionId", snapshot.projectSessionId}, {"method", method},
            {"projectSnapshotIdentity", snapshot.projectSnapshotIdentity}, {"payload", std::move(payload)}};
        if (document) { envelope["documentId"] = document->documentId; envelope["documentVersion"] = document->version; }
        std::promise<LanguageServiceResponse> promise; auto future = promise.get_future();
        { std::scoped_lock lock(mutex_); pending_.emplace(id, std::move(promise)); }
        if (!process_.WriteLine(envelope.dump()))
        {
            std::scoped_lock lock(mutex_); auto found = pending_.find(id);
            if (found != pending_.end())
            {
                LanguageServiceResponse response; response.requestId = id; response.errorCode = "LanguageService.NotRunning"; response.errorMessage = "Language service transport is unavailable.";
                found->second.set_value(std::move(response)); pending_.erase(found);
            }
        }
        return future;
    }

    void ReaderLoop()
    {
        while (running_)
        {
            auto line = process_.ReadLine(std::chrono::milliseconds{250});
            if (!line)
            {
                if (process_.State() != ToolProcessSessionState::Running) break;
                continue;
            }
            try
            {
                const auto json = Json::parse(*line);
                if (json.value("schemaVersion", 0) != 1 || json.value("protocolVersion", 0) != 1) throw std::runtime_error("unsupported protocol version");
                LanguageServiceResponse response;
                response.requestId = json.value("requestId", 0ull); response.projectSessionId = json.value("projectSessionId", "");
                response.projectSnapshotIdentity = json.value("projectSnapshotIdentity", ""); response.method = json.value("method", "");
                if (json.contains("documentId") && !json["documentId"].is_null()) response.documentId = json["documentId"].get<std::string>();
                if (json.contains("documentVersion") && !json["documentVersion"].is_null()) response.documentVersion = json["documentVersion"].get<std::uint64_t>();
                const auto payload = json.value("payload", Json::object()); response.payloadJson = payload.dump();
                try
                {
                    ParseTypedPayload(payload, response);
                }
                catch (const std::exception& error)
                {
                    response.errorCode = "LanguageService.MalformedResponse";
                    response.errorMessage = error.what();
                    std::scoped_lock lock(mutex_);
                    diagnostic_ = response.errorCode + ": " + response.errorMessage;
                }
                if (json.value("messageType", "") == "error" || json.value("messageType", "") == "fatal")
                { response.errorCode = payload.value("code", "LanguageService.ProtocolError"); response.errorMessage = payload.value("message", "Protocol error."); }
                if (json.value("messageType", "") == "notification")
                {
                    PublicationCallback callback;
                    {
                        std::scoped_lock lock(mutex_);
                        if (response.projectSnapshotIdentity ==
                            snapshot_.projectSnapshotIdentity)
                            callback = publication_;
                    }
                    if (callback) callback(std::move(response));
                }
                else
                {
                    std::scoped_lock lock(mutex_); auto found = pending_.find(response.requestId);
                    if (found != pending_.end()) { found->second.set_value(std::move(response)); pending_.erase(found); }
                }
            }
            catch (const std::exception& error)
            {
                std::scoped_lock lock(mutex_); diagnostic_ = std::string("LanguageService.MalformedResponse: ") + error.what();
            }
        }
        if (running_)
        {
            running_ = false;
            FailPending("LanguageService.ProcessExited", "Language service process exited unexpectedly.");
            SetState(monitorRunning_ && !manualStop_ ?
                LanguageServiceState::Recovering : LanguageServiceState::Unavailable);
        }
    }

    std::optional<LanguageServiceResponse> AwaitStartup(
        std::future<LanguageServiceResponse> future, const char* timeoutCode)
    {
        if (future.wait_for(std::chrono::seconds(15)) == std::future_status::ready)
            return future.get();
        FailPending(timeoutCode, "Language service startup timed out.");
        return std::nullopt;
    }

    void FailPending(const std::string& code, const std::string& message)
    {
        std::map<std::uint64_t, std::promise<LanguageServiceResponse>> pending;
        { std::scoped_lock lock(mutex_); pending.swap(pending_); }
        for (auto& [id, promise] : pending)
        {
            LanguageServiceResponse response;
            response.requestId = id;
            response.errorCode = code;
            response.errorMessage = message;
            promise.set_value(std::move(response));
        }
    }

    void MonitorLoop()
    {
        auto nextHeartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (monitorRunning_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!monitorRunning_) break;
            if (state_.load() == LanguageServiceState::Unavailable ||
                state_.load() == LanguageServiceState::Recovering)
            {
                if (!RecoverTransport()) break;
                nextHeartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                continue;
            }
            if (state_.load() != LanguageServiceState::Ready ||
                std::chrono::steady_clock::now() < nextHeartbeat) continue;
            auto heartbeat = Send("heartbeat", Json::object(), std::nullopt);
            if (heartbeat.wait_for(std::chrono::seconds(15)) != std::future_status::ready ||
                !heartbeat.get().Succeeded())
            {
                diagnostic_ = "Language service heartbeat timed out.";
                if (!RecoverTransport()) break;
            }
            nextHeartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        }
    }

    bool RecoverTransport()
    {
        std::scoped_lock lifecycleLock(lifecycleMutex_);
        if (manualStop_) return false;
        const auto now = std::chrono::steady_clock::now();
        while (!restartTimes_.empty() && now - restartTimes_.front() > std::chrono::seconds(60))
            restartTimes_.pop_front();
        if (restartTimes_.size() >= 3)
        {
            diagnostic_ = "Language service restart limit exceeded.";
            SetState(LanguageServiceState::Unavailable);
            return false;
        }
        const auto backoff = std::chrono::milliseconds(250u << restartTimes_.size());
        restartTimes_.push_back(now);
        SetState(LanguageServiceState::Recovering);
        StopTransport();
        std::this_thread::sleep_for(backoff);
        if (manualStop_) return false;
        return LaunchTransport();
    }

    static Json SnapshotJson(const LanguageServiceProjectSnapshot& value)
    {
        Json references = Json::array();
        for (const auto& reference : value.metadataReferences)
            references.push_back({{"path", reference.canonicalPath.string()},
                {"hash", reference.hash}});
        Json analyzers = Json::array();
        for (const auto& analyzer : value.analyzers)
            analyzers.push_back({{"path", analyzer.canonicalPath.string()},
                {"hash", analyzer.hash}});
        Json result{{"canonicalRoot", value.canonicalRoot.string()}, {"projectId", value.projectId},
            {"manifestHash", value.manifestHash}, {"lockHash", value.lockHash}, {"packageGeneration", value.packageGeneration},
            {"packageGenerationId", value.packageGenerationId},
            {"packageGraphHash", value.packageGraphHash},
            {"referenceManifestHash", value.referenceManifestHash}, {"ownershipManifestHash", value.ownershipManifestHash},
            {"runtimeBridgeIdentity", value.runtimeBridgeIdentity},
            {"metadataReferences", std::move(references)},
            {"analyzers", std::move(analyzers)},
            {"compilerVersion", value.compilerVersion}, {"languageVersion", value.languageVersion}};
        const auto addInput = [&result](const char* name,
            const std::optional<LanguageServiceVerifiedInput>& input)
        {
            if (input)
                result[name] = {{"path", input->canonicalPath.string()},
                    {"hash", input->hash}};
        };
        addInput("generationManifest", value.generationManifest);
        addInput("referenceManifest", value.referenceManifest);
        addInput("ownershipManifest", value.ownershipManifest);
        return result;
    }
    void SetState(LanguageServiceState value)
    {
        state_ = value; StateCallback callback; { std::scoped_lock lock(mutex_); callback = stateCallback_; } if (callback) callback(value);
    }

    ManagedToolRuntime runtime_; ToolProcessSession process_; LanguageServiceProjectSnapshot snapshot_;
    std::atomic<bool> running_{false}; std::atomic<bool> monitorRunning_{false};
    std::atomic<bool> manualStop_{true}; std::atomic<std::uint64_t> nextRequest_{1};
    std::atomic<LanguageServiceState> state_{LanguageServiceState::Stopped};
    mutable std::mutex mutex_; std::mutex lifecycleMutex_; std::mutex sendMutex_;
    std::thread reader_, monitor_;
    std::map<std::uint64_t, std::promise<LanguageServiceResponse>> pending_;
    std::map<std::string, ReplayDocument> replayDocuments_;
    std::deque<std::chrono::steady_clock::time_point> restartTimes_;
    PublicationCallback publication_; StateCallback stateCallback_; std::string diagnostic_;
};

SagaScriptLanguageServiceClient::SagaScriptLanguageServiceClient(ManagedToolRuntimeOptions runtime) : impl_(std::make_unique<Impl>(std::move(runtime))) {}
SagaScriptLanguageServiceClient::~SagaScriptLanguageServiceClient() = default;
bool SagaScriptLanguageServiceClient::Start(const LanguageServiceProjectSnapshot& snapshot) { return impl_->Start(snapshot); }
void SagaScriptLanguageServiceClient::Stop() { impl_->Stop(); }
std::future<LanguageServiceResponse> SagaScriptLanguageServiceClient::OpenDocument(const LanguageServiceDocumentIdentity& d, std::string t) { return impl_->Document("document/open", d, std::move(t)); }
std::future<LanguageServiceResponse> SagaScriptLanguageServiceClient::ChangeDocument(const LanguageServiceDocumentIdentity& d, std::string t) { return impl_->Document("document/change", d, std::move(t)); }
std::future<LanguageServiceResponse> SagaScriptLanguageServiceClient::SaveDocument(const LanguageServiceDocumentIdentity& d, std::string t) { return impl_->Document("document/save", d, std::move(t)); }
std::future<LanguageServiceResponse> SagaScriptLanguageServiceClient::CloseDocument(const LanguageServiceDocumentIdentity& d) { return impl_->Document("document/close", d, {}); }
std::future<LanguageServiceResponse> SagaScriptLanguageServiceClient::Request(std::string m, const LanguageServiceDocumentIdentity& d, std::optional<LanguageServicePosition> p, std::string x) { return impl_->Request(std::move(m), d, p, std::move(x)); }
std::future<LanguageServiceResponse> SagaScriptLanguageServiceClient::Preflight(const LanguageServiceDocumentIdentity& d, std::string candidate) { return impl_->Request("preflight", d, std::nullopt, Json{{"candidateText", std::move(candidate)}}.dump()); }
void SagaScriptLanguageServiceClient::Cancel(std::uint64_t id) { impl_->Cancel(id); }
bool SagaScriptLanguageServiceClient::Reconfigure(LanguageServiceProjectSnapshot snapshot) { return impl_->Reconfigure(std::move(snapshot)); }
void SagaScriptLanguageServiceClient::SetPublicationCallback(PublicationCallback c) { impl_->SetPublicationCallback(std::move(c)); }
void SagaScriptLanguageServiceClient::SetStateCallback(StateCallback c) { impl_->SetStateCallback(std::move(c)); }
LanguageServiceState SagaScriptLanguageServiceClient::State() const noexcept { return impl_->State(); }
std::uint64_t SagaScriptLanguageServiceClient::ProcessId() const noexcept { return impl_->ProcessId(); }
std::string SagaScriptLanguageServiceClient::LastDiagnostic() const { return impl_->Diagnostic(); }

} // namespace SagaEditor
