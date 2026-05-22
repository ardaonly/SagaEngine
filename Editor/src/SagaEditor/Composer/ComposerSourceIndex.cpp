/// @file ComposerSourceIndex.cpp
/// @brief Conservative saga.editor source index implementation.

#include "SagaEditor/Composer/ComposerSourceIndex.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace SagaEditor
{
namespace
{

[[nodiscard]] bool IsIdChar(char c) noexcept
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 ||
           c == '_' || c == '.' || c == '-';
}

[[nodiscard]] std::string Trim(std::string value)
{
    const auto begin = std::find_if_not(value.begin(),
                                        value.end(),
                                        [](unsigned char c)
                                        {
                                            return std::isspace(c) != 0;
                                        });
    const auto end = std::find_if_not(value.rbegin(),
                                      value.rend(),
                                      [](unsigned char c)
                                      {
                                          return std::isspace(c) != 0;
                                      }).base();
    if (begin >= end)
    {
        return {};
    }
    return std::string(begin, end);
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::size_t LineForOffset(const std::string& text,
                                        std::size_t offset)
{
    return static_cast<std::size_t>(
        std::count(text.begin(),
                   text.begin() + static_cast<std::ptrdiff_t>(offset),
                   '\n') + 1);
}

[[nodiscard]] ComposerSourceKind KindForModel(const std::string& model)
{
    static const std::unordered_map<std::string, ComposerSourceKind> kKinds = {
        {"EditorComposition", ComposerSourceKind::Composition},
        {"EditorPanel", ComposerSourceKind::Panel},
        {"EditorAction", ComposerSourceKind::Action},
        {"EditorMenu", ComposerSourceKind::Menu},
        {"EditorMenuItem", ComposerSourceKind::MenuItem},
        {"EditorToolbar", ComposerSourceKind::Toolbar},
        {"EditorShortcut", ComposerSourceKind::Shortcut},
        {"EditorWorkspaceLayout", ComposerSourceKind::WorkspaceLayout},
        {"EditorWorkspace", ComposerSourceKind::Workspace},
        {"EditorMode", ComposerSourceKind::Mode},
    };

    const auto it = kKinds.find(model);
    return it != kKinds.end() ? it->second : ComposerSourceKind::Unknown;
}

void ParseFields(const std::string& text,
                 std::size_t blockStart,
                 std::size_t blockEnd,
                 ComposerSourceItem& item)
{
    std::size_t lineStart = blockStart;
    while (lineStart < blockEnd)
    {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos || lineEnd > blockEnd)
        {
            lineEnd = blockEnd;
        }

        std::string line = text.substr(lineStart, lineEnd - lineStart);
        const std::size_t comment = line.find("//");
        if (comment != std::string::npos)
        {
            line.resize(comment);
        }

        const std::size_t equals = line.find('=');
        if (equals != std::string::npos)
        {
            std::string name = Trim(line.substr(0, equals));
            if (!name.empty() &&
                std::all_of(name.begin(), name.end(), IsIdChar))
            {
                std::size_t valueLocalStart = equals + 1;
                while (valueLocalStart < line.size() &&
                       std::isspace(static_cast<unsigned char>(
                           line[valueLocalStart])) != 0)
                {
                    ++valueLocalStart;
                }

                std::size_t valueLocalEnd = line.size();
                while (valueLocalEnd > valueLocalStart &&
                       std::isspace(static_cast<unsigned char>(
                           line[valueLocalEnd - 1])) != 0)
                {
                    --valueLocalEnd;
                }

                if (valueLocalStart < valueLocalEnd &&
                    line[valueLocalStart] != '[')
                {
                    ComposerSourceField field;
                    field.name = std::move(name);
                    field.line = LineForOffset(text, lineStart);
                    field.valueStart = lineStart + valueLocalStart;
                    field.valueEnd = lineStart + valueLocalEnd;
                    field.quoted = line[valueLocalStart] == '"' &&
                                   line[valueLocalEnd - 1] == '"';
                    if (field.quoted && valueLocalEnd >= valueLocalStart + 2)
                    {
                        field.value = line.substr(valueLocalStart + 1,
                                                  valueLocalEnd -
                                                      valueLocalStart - 2);
                    }
                    else
                    {
                        field.value = Trim(line.substr(
                            valueLocalStart,
                            valueLocalEnd - valueLocalStart));
                    }
                    item.fields.push_back(std::move(field));
                }
            }
        }

        lineStart = lineEnd + 1;
    }
}

void ParseFile(const std::filesystem::path& path, ComposerSourceIndex& index)
{
    const std::string text = ReadFile(path);
    std::size_t cursor = 0;
    while (cursor < text.size())
    {
        const std::size_t instancePos = text.find("instance", cursor);
        if (instancePos == std::string::npos)
        {
            break;
        }

        const bool boundaryBefore = instancePos == 0 ||
            !IsIdChar(text[instancePos - 1]);
        const std::size_t afterKeyword = instancePos + 8;
        const bool boundaryAfter = afterKeyword >= text.size() ||
            !IsIdChar(text[afterKeyword]);
        if (!boundaryBefore || !boundaryAfter)
        {
            cursor = afterKeyword;
            continue;
        }

        std::size_t pos = afterKeyword;
        while (pos < text.size() &&
               std::isspace(static_cast<unsigned char>(text[pos])) != 0)
        {
            ++pos;
        }

        const std::size_t modelStart = pos;
        while (pos < text.size() && IsIdChar(text[pos]))
        {
            ++pos;
        }
        const std::string model = text.substr(modelStart, pos - modelStart);

        while (pos < text.size() &&
               std::isspace(static_cast<unsigned char>(text[pos])) != 0)
        {
            ++pos;
        }

        const std::size_t idStart = pos;
        while (pos < text.size() && IsIdChar(text[pos]))
        {
            ++pos;
        }
        const std::string id = text.substr(idStart, pos - idStart);

        const std::size_t openBrace = text.find('{', pos);
        if (model.empty() || id.empty() || openBrace == std::string::npos)
        {
            cursor = pos;
            continue;
        }

        std::size_t depth = 0;
        std::size_t closeBrace = std::string::npos;
        for (std::size_t i = openBrace; i < text.size(); ++i)
        {
            if (text[i] == '{')
            {
                ++depth;
            }
            else if (text[i] == '}')
            {
                --depth;
                if (depth == 0)
                {
                    closeBrace = i;
                    break;
                }
            }
        }

        if (closeBrace == std::string::npos)
        {
            index.diagnostics.push_back({
                "ComposerSource.UnclosedInstance",
                "Source instance block is missing a closing brace.",
                path,
                LineForOffset(text, instancePos),
            });
            break;
        }

        ComposerSourceItem item;
        item.kind = KindForModel(model);
        item.model = model;
        item.id = id;
        item.file = path;
        item.line = LineForOffset(text, instancePos);
        item.blockStart = openBrace;
        item.blockEnd = closeBrace + 1;
        ParseFields(text, openBrace, closeBrace, item);
        if (item.kind != ComposerSourceKind::Unknown)
        {
            index.items.push_back(std::move(item));
        }

        cursor = closeBrace + 1;
    }
}

} // namespace

std::string ComposerSourceKindLabel(ComposerSourceKind kind)
{
    switch (kind)
    {
    case ComposerSourceKind::Composition: return "Composition";
    case ComposerSourceKind::Panel: return "Panels";
    case ComposerSourceKind::Action: return "Actions";
    case ComposerSourceKind::Menu: return "Menus";
    case ComposerSourceKind::MenuItem: return "Menu Items";
    case ComposerSourceKind::Toolbar: return "Toolbars";
    case ComposerSourceKind::Shortcut: return "Shortcuts";
    case ComposerSourceKind::WorkspaceLayout: return "Workspace Layouts";
    case ComposerSourceKind::Workspace: return "Workspaces";
    case ComposerSourceKind::Mode: return "Modes";
    case ComposerSourceKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

ComposerSourceIndex BuildComposerSourceIndex(const ComposerWorkspacePaths& paths)
{
    ComposerSourceIndex index;
    const std::filesystem::path sourceDir = paths.sourceRoot / "source";
    if (!std::filesystem::exists(sourceDir))
    {
        index.diagnostics.push_back({
            "ComposerSource.MissingSourceRoot",
            "Editor composition source directory does not exist.",
            sourceDir,
            0,
        });
        return index;
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(sourceDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sde")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const std::filesystem::path& file : files)
    {
        ParseFile(file, index);
    }

    std::sort(index.items.begin(),
              index.items.end(),
              [](const ComposerSourceItem& lhs,
                 const ComposerSourceItem& rhs)
              {
                  if (lhs.kind != rhs.kind)
                  {
                      return static_cast<int>(lhs.kind) <
                             static_cast<int>(rhs.kind);
                  }
                  return lhs.id < rhs.id;
              });
    return index;
}

const ComposerSourceItem* FindComposerSourceItem(
    const ComposerSourceIndex& index,
    const std::string& id)
{
    const auto it = std::find_if(index.items.begin(),
                                 index.items.end(),
                                 [&id](const ComposerSourceItem& item)
                                 {
                                     return item.id == id;
                                 });
    return it != index.items.end() ? &*it : nullptr;
}

const ComposerSourceField* FindComposerSourceField(
    const ComposerSourceItem& item,
    const std::string& fieldName)
{
    const auto it = std::find_if(item.fields.begin(),
                                 item.fields.end(),
                                 [&fieldName](const ComposerSourceField& field)
                                 {
                                     return field.name == fieldName;
                                 });
    return it != item.fields.end() ? &*it : nullptr;
}

} // namespace SagaEditor
