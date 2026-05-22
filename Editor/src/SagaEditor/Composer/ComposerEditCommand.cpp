/// @file ComposerEditCommand.cpp
/// @brief Pending source edit and checkpoint writer implementation.

#include "SagaEditor/Composer/ComposerEditCommand.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_set>

namespace SagaEditor
{
namespace
{

[[nodiscard]] bool IsBooleanText(const std::string& value) noexcept
{
    return value == "true" || value == "false";
}

[[nodiscard]] bool IsSupportedField(const ComposerSourceItem& item,
                                    const std::string& fieldName)
{
    if (fieldName == "displayName")
    {
        return item.kind == ComposerSourceKind::Composition ||
               item.kind == ComposerSourceKind::Panel ||
               item.kind == ComposerSourceKind::Action ||
               item.kind == ComposerSourceKind::Menu ||
               item.kind == ComposerSourceKind::Toolbar ||
               item.kind == ComposerSourceKind::WorkspaceLayout ||
               item.kind == ComposerSourceKind::Workspace ||
               item.kind == ComposerSourceKind::Mode;
    }
    if (fieldName == "defaultVisible")
    {
        return item.kind == ComposerSourceKind::Panel;
    }
    return false;
}

[[nodiscard]] std::string ReplacementText(const ComposerPendingEdit& edit)
{
    if (!edit.quoted)
    {
        return edit.newValue;
    }
    std::string escaped;
    escaped.reserve(edit.newValue.size() + 2);
    escaped.push_back('"');
    for (char c : edit.newValue)
    {
        if (c == '"' || c == '\\')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

[[nodiscard]] std::string Timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return stream.str();
}

[[nodiscard]] std::filesystem::path MakeCheckpointPath(
    const ComposerWorkspacePaths& paths,
    const std::filesystem::path& sourceFile)
{
    const std::filesystem::path relative =
        std::filesystem::relative(sourceFile, paths.root);
    std::filesystem::path safeName;
    for (const auto& part : relative)
    {
        if (!safeName.empty())
        {
            safeName += "__";
        }
        safeName += part;
    }
    return paths.checkpointRoot /
           (Timestamp() + "__" + safeName.filename().string() + ".bak");
}

} // namespace

void ComposerEditSession::Reset(ComposerSourceIndex index)
{
    m_index = std::move(index);
    m_pendingEdits.clear();
}

ComposerEditResult ComposerEditSession::SetField(std::string itemId,
                                                 std::string fieldName,
                                                 std::string newValue)
{
    const ComposerSourceItem* item = FindComposerSourceItem(m_index, itemId);
    if (item == nullptr)
    {
        return {false,
                "ComposerEdit.UnknownItem",
                "The selected source item does not exist.",
                m_pendingEdits};
    }

    const ComposerSourceField* field = FindComposerSourceField(*item, fieldName);
    if (field == nullptr)
    {
        return {false,
                "ComposerEdit.UnknownField",
                "The selected source item does not contain the requested field.",
                m_pendingEdits};
    }

    ComposerEditResult validation = ValidateEdit(*item, *field, newValue);
    if (!validation.ok)
    {
        validation.pendingEdits = m_pendingEdits;
        return validation;
    }

    ComposerPendingEdit edit;
    edit.file = item->file;
    edit.model = item->model;
    edit.itemId = item->id;
    edit.fieldName = field->name;
    edit.oldValue = field->value;
    edit.newValue = std::move(newValue);
    edit.valueStart = field->valueStart;
    edit.valueEnd = field->valueEnd;
    edit.quoted = field->quoted;

    const auto existing = std::find_if(
        m_pendingEdits.begin(),
        m_pendingEdits.end(),
        [&edit](const ComposerPendingEdit& pending)
        {
            return pending.itemId == edit.itemId &&
                   pending.fieldName == edit.fieldName;
        });
    if (existing != m_pendingEdits.end())
    {
        *existing = std::move(edit);
    }
    else
    {
        m_pendingEdits.push_back(std::move(edit));
    }

    return {true,
            "ComposerEdit.Pending",
            "Pending source edit created.",
            m_pendingEdits};
}

void ComposerEditSession::Revert()
{
    m_pendingEdits.clear();
}

ComposerEditResult ComposerEditSession::Save(
    const ComposerWorkspacePaths& paths)
{
    if (m_pendingEdits.empty())
    {
        return {true,
                "ComposerEdit.NoChanges",
                "There are no pending source edits.",
                m_pendingEdits};
    }

    std::map<std::filesystem::path, std::vector<ComposerPendingEdit>> byFile;
    for (const ComposerPendingEdit& edit : m_pendingEdits)
    {
        byFile[edit.file].push_back(edit);
    }

    try
    {
        std::filesystem::create_directories(paths.checkpointRoot);
        for (auto& [file, edits] : byFile)
        {
            std::sort(edits.begin(),
                      edits.end(),
                      [](const ComposerPendingEdit& lhs,
                         const ComposerPendingEdit& rhs)
                      {
                          return lhs.valueStart > rhs.valueStart;
                      });

            const std::string original = ReadFile(file);
            const std::filesystem::path checkpoint =
                MakeCheckpointPath(paths, file);
            std::filesystem::copy_file(
                file,
                checkpoint,
                std::filesystem::copy_options::overwrite_existing);

            std::string updated = original;
            for (const ComposerPendingEdit& edit : edits)
            {
                updated.replace(edit.valueStart,
                                edit.valueEnd - edit.valueStart,
                                ReplacementText(edit));
            }
            WriteFile(file, updated);
        }
    }
    catch (const std::exception& exception)
    {
        return {false,
                "ComposerEdit.WriteFailed",
                exception.what(),
                m_pendingEdits};
    }

    m_pendingEdits.clear();
    return {true,
            "ComposerEdit.Saved",
            "Source edits saved with checkpoints.",
            m_pendingEdits};
}

const std::vector<ComposerPendingEdit>&
ComposerEditSession::PendingEdits() const noexcept
{
    return m_pendingEdits;
}

ComposerEditResult ComposerEditSession::ValidateEdit(
    const ComposerSourceItem& item,
    const ComposerSourceField& field,
    const std::string& newValue) const
{
    if (!IsSupportedField(item, field.name))
    {
        return {false,
                "ComposerEdit.UnsupportedField",
                "This source field is not editable in Composer MVP.",
                {}};
    }
    if (field.name == "defaultVisible" && !IsBooleanText(newValue))
    {
        return {false,
                "ComposerEdit.InvalidBoolean",
                "defaultVisible must be true or false.",
                {}};
    }
    if (field.name == "displayName" && newValue.empty())
    {
        return {false,
                "ComposerEdit.EmptyDisplayName",
                "displayName cannot be empty.",
                {}};
    }
    return {true, "ComposerEdit.Valid", "Edit is valid.", {}};
}

} // namespace SagaEditor
