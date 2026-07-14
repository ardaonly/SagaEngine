/// @file FirstPlayablePublicClaimAudit.cpp
#include "FirstPlayable/FirstPlayablePublicClaimAudit.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>

namespace SagaProduct
{
namespace
{
const std::vector<std::string> kNonClaims = {
    "No project creation workflow claim",
    "No full editor claim",
    "No Visual Blocks canvas claim",
    "No Visual Blocks runtime graph claim",
    "No generated C# from blocks claim",
    "No multiplayer claim",
    "No package install or distribution claim",
    "No production readiness claim",
};
const std::array<std::filesystem::path, 3> kDocuments = {
    "README.md", "ACCEPTANCE.md", "KNOWN_LIMITATIONS.md"};
const std::array<std::string, 18> kForbidden = {
    "production ready", "production-ready", "full editor is complete",
    "full editor complete", "complete editor is available",
    "project creation workflow complete", "project creation workflow is complete",
    "visual blocks canvas is implemented", "visual blocks runtime graph is implemented",
    "generated c# from blocks is implemented", "multiplayer ready",
    "networking ready", "server-authoritative multiplayer complete",
    "package install complete", "package distribution complete", "distribution ready",
    "complete game", "mmo ready"};

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool Negated(const std::string& line, std::size_t match)
{
    const auto prefix = line.substr(0, match);
    for (const char* token : {"no ", "not ", "does not ", "do not ",
                              "without ", "non-claim", "remains unsupported"})
        if (prefix.rfind(token) != std::string::npos) return true;
    return false;
}
void Fail(FirstPlayablePublicClaimAuditResult& result, std::string code,
          std::string message, const std::filesystem::path& path)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = std::move(code); diagnostic.message = std::move(message);
    diagnostic.path = path;
    diagnostic.actionHint = "Replace affirmative claims with exact evidence or an explicit non-claim.";
    result.diagnostics.push_back(std::move(diagnostic));
    result.status = EvidenceStatus::Failed;
}
} // namespace

const std::vector<std::string>& FirstPlayablePublicClaimAudit::CanonicalNonClaims()
{
    return kNonClaims;
}

FirstPlayablePublicClaimAuditResult FirstPlayablePublicClaimAudit::Run(
    const std::filesystem::path& projectRoot)
{
    FirstPlayablePublicClaimAuditResult result;
    result.status = EvidenceStatus::Passed;
    result.nonClaims = kNonClaims;
    std::string corpus;
    for (const auto& relative : kDocuments)
    {
        const auto path = projectRoot / relative;
        std::ifstream input(path);
        if (!input)
        {
            Fail(result, "ProductShell.FirstPlayable.PublicClaimScanFailed",
                 "public claim document is missing", path);
            continue;
        }
        std::string line;
        while (std::getline(input, line))
        {
            const std::string lower = Lower(line);
            corpus += lower + '\n';
            for (const auto& phrase : kForbidden)
            {
                const auto match = lower.find(phrase);
                if (match != std::string::npos && !Negated(lower, match))
                    Fail(result, "ProductShell.FirstPlayable.ForbiddenPublicClaim",
                         "forbidden affirmative public claim: " + phrase, path);
            }
        }
    }
    for (const auto& claim : kNonClaims)
        if (corpus.find(Lower(claim)) == std::string::npos)
            Fail(result, "ProductShell.FirstPlayable.PublicClaimScanFailed",
                 "canonical non-claim is missing from public docs: " + claim,
                 projectRoot);
    return result;
}
} // namespace SagaProduct
