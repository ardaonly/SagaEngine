#include "SagaEditor/Authoring/ScriptBehaviorProjectionView.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace SagaEditor::Authoring
{
namespace
{

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[nodiscard]] fs::path ArtifactRoot(const ProjectReadinessView& project)
{
    return (project.projectRoot / "Build" / "SagaScript").lexically_normal();
}

[[nodiscard]] std::string StringField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}

[[nodiscard]] bool BoolField(const Json& object, const char* key, const bool fallback)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_boolean())
    {
        return fallback;
    }
    return it->get<bool>();
}

[[nodiscard]] int IntField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer())
    {
        return 0;
    }
    return it->get<int>();
}

[[nodiscard]] ScriptSourceSpanView ParseSpan(const Json& object)
{
    return ScriptSourceSpanView{
        IntField(object, "startLine"),
        IntField(object, "startColumn"),
        IntField(object, "endLine"),
        IntField(object, "endColumn"),
        IntField(object, "startByte"),
        IntField(object, "endByte"),
    };
}

[[nodiscard]] Json ReadJsonFile(const fs::path& path,
                                std::vector<AuthoringDiagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Missing",
            "script artifact is missing",
            path.string(),
        });
        return Json();
    }
    try
    {
        Json json;
        input >> json;
        return json;
    }
    catch (const Json::exception& error)
    {
        diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Invalid",
            error.what(),
            path.string(),
        });
        return Json();
    }
}

[[nodiscard]] fs::path ResolveSourcePath(const ProjectReadinessView& project,
                                         const std::string& value)
{
    fs::path path(value);
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }
    return (project.projectRoot / path).lexically_normal();
}

[[nodiscard]] std::string Sha256Hex(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return {};
    }

    using EvpContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    EvpContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    {
        return {};
    }

    std::array<char, 8192> buffer{};
    while (input.good())
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 &&
            EVP_DigestUpdate(
                context.get(),
                buffer.data(),
                static_cast<std::size_t>(count)) != 1)
        {
            return {};
        }
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (input.bad() ||
        EVP_DigestFinal_ex(context.get(), digest.data(), &digestLength) != 1)
    {
        return {};
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLength; ++i)
    {
        output << std::setw(2) << static_cast<int>(digest[i]);
    }
    return output.str();
}

[[nodiscard]] ScriptArtifactStatus SourceFreshness(
    const fs::path& sourceFile,
    const std::string& expectedHash)
{
    if (expectedHash.empty())
    {
        return ScriptArtifactStatus::UnknownFreshness;
    }
    if (!fs::exists(sourceFile))
    {
        return ScriptArtifactStatus::MissingSource;
    }
    return Sha256Hex(sourceFile) == expectedHash
        ? ScriptArtifactStatus::Ready
        : ScriptArtifactStatus::Stale;
}

[[nodiscard]] ScriptSourceLinkView BuildSourceLink(
    const ProjectReadinessView& project,
    const std::string& behaviorId,
    const std::string& nodeId,
    const std::string& sourceFile,
    const std::string& sourceHash,
    const Json* span)
{
    ScriptSourceLinkView link;
    link.behaviorId = behaviorId;
    link.nodeId = nodeId;
    link.sourceFile = ResolveSourcePath(project, sourceFile);
    link.sourceHash = sourceHash;
    link.freshness = SourceFreshness(link.sourceFile, sourceHash);
    if (span != nullptr && span->is_object())
    {
        link.span = ParseSpan(*span);
    }
    return link;
}

[[nodiscard]] std::vector<ScriptSourceLinkView> LoadLinksFromSourceMap(
    const ProjectReadinessView& project,
    const Json& sourceMap)
{
    std::vector<ScriptSourceLinkView> links;
    const auto behaviorsIt = sourceMap.find("behaviors");
    if (behaviorsIt == sourceMap.end() || !behaviorsIt->is_array())
    {
        return links;
    }

    for (const Json& behavior : *behaviorsIt)
    {
        if (!behavior.is_object())
        {
            continue;
        }
        const std::string behaviorId = StringField(behavior, "behaviorId");
        const std::string sourceFile = StringField(behavior, "sourceFile");
        const std::string sourceHash = StringField(behavior, "sourceHash");
        const auto spanIt = behavior.find("sourceSpan");
        links.push_back(BuildSourceLink(
            project,
            behaviorId,
            {},
            sourceFile,
            sourceHash,
            spanIt == behavior.end() ? nullptr : &*spanIt));

        const auto nodesIt = behavior.find("nodes");
        if (nodesIt == behavior.end() || !nodesIt->is_array())
        {
            continue;
        }
        for (const Json& node : *nodesIt)
        {
            if (!node.is_object())
            {
                continue;
            }
            const auto nodeSpanIt = node.find("sourceSpan");
            links.push_back(BuildSourceLink(
                project,
                behaviorId,
                StringField(node, "nodeId"),
                sourceFile,
                sourceHash,
                nodeSpanIt == node.end() ? nullptr : &*nodeSpanIt));
        }
    }
    return links;
}

} // namespace

ScriptBehaviorProjectionLoadResult LoadScriptSourceLinkViews(
    const ProjectReadinessView& project)
{
    ScriptBehaviorProjectionLoadResult result;
    if (!project.ok)
    {
        result.diagnostics = project.diagnostics;
        return result;
    }

    const Json sourceMap = ReadJsonFile(
        ArtifactRoot(project) / "source_map.json",
        result.diagnostics);
    if (!sourceMap.is_object())
    {
        return result;
    }

    result.sourceLinks = LoadLinksFromSourceMap(project, sourceMap);
    result.ok = result.diagnostics.empty();
    return result;
}

ScriptBehaviorProjectionLoadResult LoadScriptBehaviorProjectionViews(
    const ProjectReadinessView& project)
{
    ScriptBehaviorProjectionLoadResult result = LoadScriptSourceLinkViews(project);
    if (!project.ok)
    {
        return result;
    }

    const Json projection = ReadJsonFile(
        ArtifactRoot(project) / "projection_report.json",
        result.diagnostics);
    if (!projection.is_object())
    {
        result.ok = false;
        return result;
    }

    std::unordered_map<std::string, ScriptSourceLinkView> linksByKey;
    for (const ScriptSourceLinkView& link : result.sourceLinks)
    {
        linksByKey[link.behaviorId + "\n" + link.nodeId] = link;
    }

    const auto behaviorsIt = projection.find("behaviors");
    if (behaviorsIt == projection.end() || !behaviorsIt->is_array())
    {
        result.ok = result.diagnostics.empty();
        return result;
    }

    for (const Json& behavior : *behaviorsIt)
    {
        if (!behavior.is_object())
        {
            continue;
        }

        ScriptBehaviorProjectionView view;
        view.behaviorId = StringField(behavior, "behaviorId");
        view.apiLevel = StringField(behavior, "apiLevel");
        view.apiDomain = StringField(behavior, "apiDomain");
        view.compatibility = StringField(behavior, "compatibility");
        const auto behaviorLinkIt = linksByKey.find(view.behaviorId + "\n");
        if (behaviorLinkIt != linksByKey.end())
        {
            view.sourceLink = behaviorLinkIt->second;
        }

        const auto nodesIt = behavior.find("nodes");
        if (nodesIt != behavior.end() && nodesIt->is_array())
        {
            for (const Json& node : *nodesIt)
            {
                if (!node.is_object())
                {
                    continue;
                }

                ScriptBlockProjectionNodeView nodeView;
                nodeView.nodeId = StringField(node, "nodeId");
                nodeView.kind = StringField(node, "kind");
                nodeView.displayName = StringField(node, "displayName");
                nodeView.apiLevel = StringField(node, "apiLevel");
                nodeView.apiDomain = StringField(node, "apiDomain");
                nodeView.capability = StringField(node, "capability");
                nodeView.projectionCompatibility =
                    StringField(node, "projectionCompatibility");
                nodeView.readOnly = BoolField(node, "readOnly", true);
                nodeView.opaqueReason = StringField(node, "opaqueReason");
                nodeView.literalValue = StringField(node, "literalValue");
                const auto nodeLinkIt =
                    linksByKey.find(view.behaviorId + "\n" + nodeView.nodeId);
                if (nodeLinkIt != linksByKey.end())
                {
                    nodeView.sourceLink = nodeLinkIt->second;
                }
                view.nodes.push_back(nodeView);
            }
        }
        result.behaviors.push_back(view);
    }

    result.ok = result.diagnostics.empty();
    return result;
}

} // namespace SagaEditor::Authoring
