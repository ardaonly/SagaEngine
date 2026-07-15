/// @file EditorQtPublicAbiBoundaryTests.cpp
/// @brief Guards public Editor headers against new Qt ABI exposure.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct Finding
{
    std::string file;
    std::size_t line = 0;
    std::string token;
    std::string text;
};

std::string Trim(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
    {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

bool StartsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

bool IsIdentifierChar(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
           value == '_';
}

bool ContainsWord(std::string_view text, std::string_view word)
{
    std::size_t cursor = text.find(word);
    while (cursor != std::string_view::npos)
    {
        const bool leftBoundary = cursor == 0 ||
                                  !IsIdentifierChar(text[cursor - 1]);
        const auto right = cursor + word.size();
        const bool rightBoundary = right >= text.size() ||
                                   !IsIdentifierChar(text[right]);
        if (leftBoundary && rightBoundary)
        {
            return true;
        }

        cursor = text.find(word, cursor + 1);
    }

    return false;
}

bool ContainsWordWithPrefix(std::string_view text, std::string_view prefix)
{
    std::size_t cursor = text.find(prefix);
    while (cursor != std::string_view::npos)
    {
        const bool leftBoundary = cursor == 0 ||
                                  !IsIdentifierChar(text[cursor - 1]);
        if (leftBoundary)
        {
            return true;
        }

        cursor = text.find(prefix, cursor + 1);
    }

    return false;
}

std::string StripComments(std::string_view line, bool& inBlockComment)
{
    std::string result;
    for (std::size_t i = 0; i < line.size();)
    {
        if (inBlockComment)
        {
            const auto end = line.find("*/", i);
            if (end == std::string_view::npos)
            {
                return result;
            }

            inBlockComment = false;
            i = end + 2;
            continue;
        }

        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/')
        {
            break;
        }

        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '*')
        {
            inBlockComment = true;
            i += 2;
            continue;
        }

        result.push_back(line[i]);
        ++i;
    }

    return result;
}

std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

bool IsHeaderFile(const std::filesystem::path& path)
{
    const auto extension = path.extension().string();
    return extension == ".h" || extension == ".hpp" ||
           extension == ".hh" || extension == ".hxx";
}

std::string RelativeToSourceRoot(const std::filesystem::path& path)
{
    return std::filesystem::relative(
               path, std::filesystem::path(SAGA_SOURCE_ROOT))
        .generic_string();
}

void AddFinding(std::vector<Finding>& findings,
                const std::string& file,
                std::size_t line,
                std::string token,
                const std::string& text)
{
    findings.push_back(Finding{file, line, std::move(token), Trim(text)});
}

std::vector<Finding> FindQtExposureInHeader(const std::filesystem::path& path)
{
    const auto file = RelativeToSourceRoot(path);
    const auto lines = ReadLines(path);
    std::vector<Finding> findings;
    bool inBlockComment = false;

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const auto code = StripComments(lines[i], inBlockComment);
        const auto trimmed = Trim(code);
        if (trimmed.empty())
        {
            continue;
        }

        if (StartsWith(trimmed, "#include") &&
            (trimmed.find("<Q") != std::string::npos ||
             trimmed.find("\"Q") != std::string::npos ||
             trimmed.find("<Qt") != std::string::npos ||
             trimmed.find("\"Qt") != std::string::npos))
        {
            AddFinding(findings, file, i + 1, "QtInclude", trimmed);
        }

        if (trimmed.find("SagaEditorQt/QtPanel.h") != std::string::npos)
        {
            AddFinding(findings, file, i + 1, "QtPanelInclude", trimmed);
        }

        if (ContainsWord(trimmed, "Q_OBJECT"))
        {
            AddFinding(findings, file, i + 1, "Q_OBJECT", trimmed);
        }

        const std::vector<std::string> exactTokens = {
            "QObject",
            "QWidget",
            "QMainWindow",
            "QStackedWidget",
            "QString",
            "QVariant",
            "QModelIndex",
            "QAction",
            "QMenu",
            "QDockWidget",
            "QApplication",
            "QTreeView",
            "QTableView",
            "QtCore",
            "QtGui",
            "QtWidgets",
            "QtPanel",
        };

        for (const auto& token : exactTokens)
        {
            if (ContainsWord(trimmed, token))
            {
                AddFinding(findings, file, i + 1, token, trimmed);
            }
        }

        if (ContainsWordWithPrefix(trimmed, "QAbstract"))
        {
            AddFinding(findings, file, i + 1, "QAbstract*", trimmed);
        }

        if (trimmed.find("Qt::") != std::string::npos)
        {
            AddFinding(findings, file, i + 1, "Qt::", trimmed);
        }
    }

    return findings;
}

std::vector<std::filesystem::path> PublicHeadersToScan()
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    std::vector<std::filesystem::path> headers;

    for (const auto* module : {
             "EditorCore", "EditorFramework", "EditorAuthoring",
             "VisualBlocksEditor", "EditorScripting", "EditorCollaboration",
             "EditorExperimental"})
    {
        const auto publicRoot =
            root / "Engine" / "Source" / "Editor" / module / "Public";
        if (!std::filesystem::exists(publicRoot))
        {
            continue;
        }
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(publicRoot))
        {
            if (entry.is_regular_file() && IsHeaderFile(entry.path()))
            {
                headers.push_back(entry.path());
            }
        }
    }

    std::sort(headers.begin(), headers.end());
    return headers;
}

using TokenSet = std::set<std::string>;
using Allowlist = std::map<std::string, TokenSet>;

const Allowlist& QtExposureAllowlist()
{
    static const Allowlist allowlist;
    return allowlist;
}

std::string FormatFinding(const Finding& finding)
{
    std::ostringstream output;
    output << finding.file << ":" << finding.line << ": " << finding.token
           << ": " << finding.text;
    return output.str();
}

} // namespace

TEST(EditorQtPublicAbiBoundaryTests, PublicHeadersDoNotGrowQtExposure)
{
    const auto headers = PublicHeadersToScan();
    ASSERT_FALSE(headers.empty())
        << "Editor module public headers must exist.";

    const auto& allowlist = QtExposureAllowlist();
    std::vector<Finding> allowedFindings;
    std::vector<Finding> violations;
    std::map<std::string, TokenSet> observedAllowedTokens;
    std::set<std::string> scannedFiles;

    for (const auto& header : headers)
    {
        const auto relative = RelativeToSourceRoot(header);
        scannedFiles.insert(relative);

        for (const auto& finding : FindQtExposureInHeader(header))
        {
            const auto allowlistEntry = allowlist.find(finding.file);
            if (allowlistEntry != allowlist.end() &&
                allowlistEntry->second.find(finding.token) !=
                    allowlistEntry->second.end())
            {
                allowedFindings.push_back(finding);
                observedAllowedTokens[finding.file].insert(finding.token);
                continue;
            }

            violations.push_back(finding);
        }
    }

    std::vector<std::string> missingAllowlistEntries;
    std::vector<std::string> missingAllowedTokens;
    for (const auto& [file, tokens] : allowlist)
    {
        if (scannedFiles.find(file) == scannedFiles.end())
        {
            missingAllowlistEntries.push_back(file);
            continue;
        }

        for (const auto& token : tokens)
        {
            const auto observed = observedAllowedTokens.find(file);
            if (observed == observedAllowedTokens.end() ||
                observed->second.find(token) == observed->second.end())
            {
                missingAllowedTokens.push_back(file + ": " + token);
            }
        }
    }

    std::cout << "Editor Qt public ABI boundary guard\n";
    std::cout << "Scanned public headers: " << headers.size() << "\n";
    std::cout << "Allowed Qt exposure findings: "
              << allowedFindings.size() << "\n";
    std::cout << "Violation count: " << violations.size() << "\n";
    std::cout << "Allowed entries:\n";
    for (const auto& [file, tokens] : allowlist)
    {
        std::cout << "  " << file << ":";
        for (const auto& token : tokens)
        {
            std::cout << " " << token;
        }
        std::cout << "\n";
    }

    if (!violations.empty())
    {
        std::cout << "Violations:\n";
        for (const auto& violation : violations)
        {
            std::cout << "  " << FormatFinding(violation) << "\n";
        }
    }

    EXPECT_TRUE(missingAllowlistEntries.empty())
        << "Qt exposure allowlist contains files that were not scanned. First "
           "missing entry: "
        << (missingAllowlistEntries.empty() ? "" : missingAllowlistEntries.front());

    EXPECT_TRUE(missingAllowedTokens.empty())
        << "Qt exposure allowlist contains stale token entries. First missing "
           "allowed token: "
        << (missingAllowedTokens.empty() ? "" : missingAllowedTokens.front());

    EXPECT_TRUE(violations.empty())
        << "Public Qt exposure is confined to EditorQt. First violation: "
        << (violations.empty() ? "" : FormatFinding(violations.front()));
}
