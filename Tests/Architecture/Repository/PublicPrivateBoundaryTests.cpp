#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
bool IsHeader(const std::filesystem::path& path)
{
    const auto extension = path.extension().string();
    return extension == ".h" || extension == ".hh" ||
           extension == ".hpp" || extension == ".hxx";
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string StripComments(std::string_view text)
{
    enum class State
    {
        Code,
        LineComment,
        BlockComment,
        StringLiteral,
        CharLiteral,
    };

    std::string result(text);
    State state = State::Code;
    bool escaped = false;
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const char c = result[i];
        const char next = i + 1 < result.size() ? result[i + 1] : '\0';
        switch (state)
        {
        case State::Code:
            if (c == '/' && next == '/')
            {
                result[i] = result[i + 1] = ' ';
                ++i;
                state = State::LineComment;
            }
            else if (c == '/' && next == '*')
            {
                result[i] = result[i + 1] = ' ';
                ++i;
                state = State::BlockComment;
            }
            else if (c == '"')
            {
                escaped = false;
                state = State::StringLiteral;
            }
            else if (c == '\'')
            {
                escaped = false;
                state = State::CharLiteral;
            }
            break;
        case State::LineComment:
            if (c == '\n')
            {
                state = State::Code;
            }
            else
            {
                result[i] = ' ';
            }
            break;
        case State::BlockComment:
            if (c == '*' && next == '/')
            {
                result[i] = result[i + 1] = ' ';
                ++i;
                state = State::Code;
            }
            else if (c != '\n')
            {
                result[i] = ' ';
            }
            break;
        case State::StringLiteral:
        case State::CharLiteral:
            if (!escaped &&
                ((state == State::StringLiteral && c == '"') ||
                 (state == State::CharLiteral && c == '\'')))
            {
                state = State::Code;
            }
            escaped = !escaped && c == '\\';
            if (c != '\\')
            {
                escaped = false;
            }
            break;
        }
    }
    return result;
}

std::string StripStringAndCharLiterals(std::string_view text)
{
    std::string result(text);
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    for (char& c : result)
    {
        if (!inString && !inChar)
        {
            if (c == '"')
            {
                c = ' ';
                inString = true;
            }
            else if (c == '\'')
            {
                c = ' ';
                inChar = true;
            }
            continue;
        }

        const char original = c;
        if (c != '\n')
        {
            c = ' ';
        }
        if (!escaped && ((inString && original == '"') ||
                         (inChar && original == '\'')))
        {
            inString = false;
            inChar = false;
        }
        escaped = !escaped && original == '\\';
        if (original != '\\')
        {
            escaped = false;
        }
    }
    return result;
}

bool IsIdentifierChar(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
           value == '_';
}

bool ContainsWord(std::string_view text, std::string_view word)
{
    auto cursor = text.find(word);
    while (cursor != std::string_view::npos)
    {
        const bool leftBoundary = cursor == 0 ||
                                  !IsIdentifierChar(text[cursor - 1]);
        const auto right = cursor + word.size();
        const bool rightBoundary = right == text.size() ||
                                   !IsIdentifierChar(text[right]);
        if (leftBoundary && rightBoundary)
        {
            return true;
        }
        cursor = text.find(word, cursor + 1);
    }
    return false;
}

bool ContainsQtType(std::string_view code)
{
    if (code.find("Qt::") != std::string_view::npos ||
        ContainsWord(code, "Q_OBJECT"))
    {
        return true;
    }

    for (const auto* type : {
             "QObject", "QWidget", "QMainWindow", "QStackedWidget",
             "QString", "QVariant", "QModelIndex", "QAction", "QMenu",
             "QDockWidget", "QApplication", "QTreeView", "QTableView",
             "QSettings", "QIcon", "QtCore", "QtGui", "QtWidgets"})
    {
        if (ContainsWord(code, type))
        {
            return true;
        }
    }
    auto abstractType = code.find("QAbstract");
    while (abstractType != std::string_view::npos)
    {
        if (abstractType == 0 || !IsIdentifierChar(code[abstractType - 1]))
        {
            return true;
        }
        abstractType = code.find("QAbstract", abstractType + 1);
    }
    return false;
}

std::vector<std::string> IncludeTargets(std::string_view commentsRemoved)
{
    std::istringstream lines{std::string(commentsRemoved)};
    std::vector<std::string> targets;
    std::string line;
    while (std::getline(lines, line))
    {
        std::size_t cursor = 0;
        while (cursor < line.size() &&
               std::isspace(static_cast<unsigned char>(line[cursor])) != 0)
        {
            ++cursor;
        }
        if (cursor == line.size() || line[cursor++] != '#')
        {
            continue;
        }
        while (cursor < line.size() &&
               std::isspace(static_cast<unsigned char>(line[cursor])) != 0)
        {
            ++cursor;
        }
        constexpr std::string_view directive = "include";
        if (line.compare(cursor, directive.size(), directive) != 0)
        {
            continue;
        }
        cursor += directive.size();
        while (cursor < line.size() &&
               std::isspace(static_cast<unsigned char>(line[cursor])) != 0)
        {
            ++cursor;
        }
        if (cursor == line.size() ||
            (line[cursor] != '<' && line[cursor] != '"'))
        {
            continue;
        }
        const char close = line[cursor] == '<' ? '>' : '"';
        const auto end = line.find(close, cursor + 1);
        if (end != std::string::npos)
        {
            targets.push_back(line.substr(cursor + 1, end - cursor - 1));
        }
    }
    return targets;
}

bool ContainsPrivateInclude(std::string_view commentsRemoved)
{
    for (const auto& target : IncludeTargets(commentsRemoved))
    {
        if (target.find("Private/") != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

std::string FindVendorExposure(
    std::string_view commentsRemoved,
    bool allowQt)
{
    for (const auto& include : IncludeTargets(commentsRemoved))
    {
        for (const auto* vendorRoot : {
                 "DiligentCore/", "SDL", "pqxx/", "hiredis/",
                 "sw/redis++/", "RmlUi/"})
        {
            if (include.rfind(vendorRoot, 0) == 0)
            {
                return include;
            }
        }
        const bool qtHeader = include.rfind("Qt", 0) == 0 ||
                              (include.size() > 1 && include[0] == 'Q' &&
                               std::isupper(static_cast<unsigned char>(
                                   include[1])) != 0);
        if (!allowQt && qtHeader)
        {
            return include;
        }
    }

    const auto code = StripStringAndCharLiterals(commentsRemoved);
    for (const auto* typeToken : {
             "Diligent::", "RefCntAutoPtr<", "Rml::", "pqxx::",
             "sw::redis::", "SDL_"})
    {
        if (code.find(typeToken) != std::string::npos)
        {
            return typeToken;
        }
    }
    for (const auto* hiredisType : {
             "redisContext", "redisAsyncContext", "redisReply"})
    {
        if (ContainsWord(code, hiredisType))
        {
            return hiredisType;
        }
    }
    if (!allowQt && ContainsQtType(code))
    {
        return "Qt type";
    }
    return {};
}

std::vector<std::filesystem::path> PublicHeaders()
{
    const auto sourceRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source";
    std::vector<std::filesystem::path> headers;
    for (const auto* groupName : {"Runtime", "Editor"})
    {
        const auto group = sourceRoot / groupName;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(group))
        {
            if (entry.is_regular_file() && IsHeader(entry.path()) &&
                entry.path().generic_string().find("/Public/") !=
                    std::string::npos)
            {
                headers.push_back(entry.path());
            }
        }
    }
    return headers;
}
} // namespace

TEST(PublicPrivateBoundaryTests, CanonicalPublicAndPrivateOwnersExist)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::array owners = {
        root / "Engine/Source/Runtime/Networking/Public/SagaEngine/Networking",
        root / "Engine/Source/Runtime/Networking/Private/SagaEngine/Networking",
        root / "Engine/Source/Runtime/Replication/Public/SagaEngine/Replication",
        root / "Engine/Source/Runtime/Replication/Private/SagaEngine/Replication",
        root / "Engine/Source/Runtime/ServerAuthority/Public/SagaEngine/ServerAuthority",
        root / "Engine/Source/Runtime/ServerAuthority/Private/SagaEngine/ServerAuthority",
        root / "Engine/Source/Runtime/Persistence/Public/SagaEngine/Persistence",
        root / "Engine/Source/Runtime/Persistence/Private/SagaEngine/Persistence/Backends",
        root / "Engine/Source/Runtime/UI/Public/SagaEngine/UI",
        root / "Engine/Source/Runtime/UI/Private/Backends",
        root / "Engine/Source/Editor/EditorQt/Public/SagaEditorQt",
        root / "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks",
    };
    for (const auto& owner : owners)
    {
        EXPECT_TRUE(std::filesystem::is_directory(owner)) << owner;
    }
}

TEST(PublicPrivateBoundaryTests, RuntimeAndEditorPublicHeadersDoNotIncludePrivatePaths)
{
    std::vector<std::filesystem::path> offenders;
    for (const auto& header : PublicHeaders())
    {
        const auto commentsRemoved = StripComments(ReadText(header));
        if (ContainsPrivateInclude(commentsRemoved))
        {
            offenders.push_back(header);
        }
    }
    EXPECT_TRUE(offenders.empty())
        << "A Runtime or Editor public header includes a Private path: "
        << (offenders.empty() ? "" : offenders.front().generic_string());
}

TEST(PublicPrivateBoundaryTests, VisualBlocksPublicSurfaceContainsOnlyBlocksContracts)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Engine/Source/Editor/VisualBlocksEditor";
    const auto publicVisualBlocks = root / "Public/SagaEditor/VisualBlocks";
    const auto privateVisualBlocks = root / "Private/SagaEditor/VisualBlocks";

    ASSERT_TRUE(std::filesystem::is_directory(publicVisualBlocks / "Blocks"));
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(publicVisualBlocks))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto relative =
            std::filesystem::relative(entry.path(), publicVisualBlocks);
        ASSERT_FALSE(relative.empty());
        EXPECT_EQ((*relative.begin()).string(), "Blocks") << entry.path();
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(privateVisualBlocks))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto relative =
            std::filesystem::relative(entry.path(), privateVisualBlocks);
        ASSERT_FALSE(relative.empty());
        EXPECT_EQ((*relative.begin()).string(), "Blocks") << entry.path();
    }

    EXPECT_TRUE(std::filesystem::is_regular_file(
        publicVisualBlocks / "Blocks/BlockDefinition.h"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        publicVisualBlocks / "Blocks/BlockToIRLowerer.h"));
}

TEST(PublicPrivateBoundaryTests, CollaborationConcreteImplementationsStayPrivate)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Engine/Source/Editor/EditorCollaboration";
    const auto publicCollaboration = root / "Public/SagaEditor/Collaboration";
    const auto privateCollaboration = root / "Private/SagaEditor/Collaboration";
    constexpr std::array<std::string_view, 15> concreteHeaders = {
        "Audit/AuditLogger.h",
        "Authority/AuthorityManager.h",
        "Client/CollaborationClient.h",
        "Client/PeerRegistry.h",
        "Locks/LockManager.h",
        "Permissions/PermissionManager.h",
        "Presence/PresenceManager.h",
        "Replay/OperationLog.h",
        "Server/CollaborationServer.h",
        "Server/CollaborationServerRouter.h",
        "Session/CollaborationSession.h",
        "Sync/CrdtSceneDocument.h",
        "Sync/EditOperationQueue.h",
        "Sync/SyncTransportImpl.h",
        "Workspace/CollaborativeWorkspace.h",
    };

    for (const auto relative : concreteHeaders)
    {
        EXPECT_FALSE(std::filesystem::exists(publicCollaboration / relative))
            << relative;
        EXPECT_TRUE(std::filesystem::is_regular_file(privateCollaboration / relative))
            << relative;
    }

    EXPECT_TRUE(std::filesystem::is_regular_file(
        publicCollaboration / "Client/ICollaborationClient.h"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        publicCollaboration / "Sync/EditOperation.h"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        publicCollaboration / "Workspace/WorkspaceSnapshot.h"));
}

TEST(PublicPrivateBoundaryTests, UnregisteredPlaceholderPanelsStayAbsent)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    constexpr std::array<std::string_view, 20> placeholderPanels = {
        "Engine/Source/Editor/EditorFramework/Public/SagaEditor/Panels/"
        "CollaborationPanel.h",
        "Engine/Source/Editor/EditorFramework/Public/SagaEditor/Panels/"
        "GraphViewportPanel.h",
        "Engine/Source/Editor/EditorFramework/Public/SagaEditor/Panels/"
        "ProfilerPanel.h",
        "Engine/Source/Editor/EditorFramework/Public/SagaEditor/Panels/"
        "ProjectBrowserPanel.h",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/Panels/"
        "CollaborationPanel.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/Panels/"
        "GraphViewportPanel.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/Panels/"
        "ProfilerPanel.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/Panels/"
        "ProjectBrowserPanel.cpp",
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/"
        "Debugger/ExecutionTraceView.h",
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/"
        "Debugger/GraphDebuggerView.h",
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/"
        "Editor/BreakpointPanel.h",
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/"
        "Editor/NodePalette.h",
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/"
        "Editor/WatchPanel.h",
        "Engine/Source/Editor/VisualBlocksEditor/Public/SagaEditor/VisualBlocks/"
        "Nodes/NodeLibraryPanel.h",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/VisualScripting/"
        "Debugger/ExecutionTraceView.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/VisualScripting/"
        "Debugger/GraphDebuggerView.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/VisualScripting/"
        "Editor/BreakpointPanel.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/VisualScripting/"
        "Editor/NodePalette.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/VisualScripting/"
        "Editor/WatchPanel.cpp",
        "Engine/Source/Editor/EditorQt/Private/SagaEditorQt/VisualScripting/"
        "Nodes/NodeLibraryPanel.cpp",
    };

    for (const auto path : placeholderPanels)
    {
        EXPECT_FALSE(std::filesystem::exists(root / path)) << path;
    }
}

TEST(PublicPrivateBoundaryTests, PublicHeadersDoNotExposeImplementationVendorTypes)
{
    std::vector<std::string> offenders;
    for (const auto& header : PublicHeaders())
    {
        const auto path = header.generic_string();
        const bool editorQtOwner =
            path.find("/Editor/EditorQt/Public/") != std::string::npos;
        const auto commentsRemoved = StripComments(ReadText(header));
        const auto token = FindVendorExposure(commentsRemoved, editorQtOwner);
        if (!token.empty())
        {
            offenders.push_back(path + ": " + token);
        }
    }
    EXPECT_TRUE(offenders.empty())
        << "A public header exposes an implementation vendor type/include: "
        << (offenders.empty() ? "" : offenders.front());
}
