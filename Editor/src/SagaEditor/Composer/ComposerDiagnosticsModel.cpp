/// @file ComposerDiagnosticsModel.cpp
/// @brief Diagnostics report model implementation for SagaEditorComposer.

#include "SagaEditor/Composer/ComposerDiagnosticsModel.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace SagaEditor
{
namespace
{

using Json = nlohmann::json;

[[nodiscard]] std::string StringAt(const Json& json,
                                   std::initializer_list<const char*> keys)
{
    const Json* current = &json;
    for (const char* key : keys)
    {
        if (!current->is_object())
        {
            return {};
        }
        const auto it = current->find(key);
        if (it == current->end())
        {
            return {};
        }
        current = &*it;
    }
    return current->is_string() ? current->get<std::string>() : std::string{};
}

[[nodiscard]] int IntAt(const Json& json,
                        std::initializer_list<const char*> keys)
{
    const Json* current = &json;
    for (const char* key : keys)
    {
        if (!current->is_object())
        {
            return 0;
        }
        const auto it = current->find(key);
        if (it == current->end())
        {
            return 0;
        }
        current = &*it;
    }
    return current->is_number_integer() ? current->get<int>() : 0;
}

void AddDiagnosticRow(const Json& diagnostic,
                      ComposerDiagnosticsModel& model)
{
    ComposerDiagnosticRow row;
    row.severity = diagnostic.value("severity", std::string{});
    row.code = diagnostic.value("code", std::string{});
    row.message = diagnostic.value("message", std::string{});
    row.file = StringAt(diagnostic, {"location", "resource"});
    row.line = IntAt(diagnostic, {"location", "line"});
    row.column = IntAt(diagnostic, {"location", "column"});
    if (row.file.empty())
    {
        row.file = diagnostic.value("sourceFile", std::string{});
    }
    if (row.line == 0)
    {
        row.line = diagnostic.value("line", 0);
    }
    if (row.column == 0)
    {
        row.column = diagnostic.value("column", 0);
    }
    model.rows.push_back(std::move(row));
}

} // namespace

ComposerDiagnosticsModel LoadComposerDiagnostics(
    const ComposerWorkspacePaths& paths)
{
    ComposerDiagnosticsModel model;
    model.reportFound = std::filesystem::exists(paths.diagnosticsPath);
    if (!model.reportFound)
    {
        model.message = "No diagnostics report found. Run Build Composition.";
        return model;
    }

    try
    {
        std::ifstream input(paths.diagnosticsPath);
        Json root;
        input >> root;
        if (root.is_array())
        {
            for (const Json& diagnostic : root)
            {
                AddDiagnosticRow(diagnostic, model);
            }
        }
        else if (root.is_object() && root.contains("diagnostics") &&
                 root["diagnostics"].is_array())
        {
            for (const Json& diagnostic : root["diagnostics"])
            {
                AddDiagnosticRow(diagnostic, model);
            }
        }
        model.message = model.rows.empty()
            ? "Diagnostics report is clean."
            : "Diagnostics report loaded.";
    }
    catch (const std::exception& exception)
    {
        model.message = exception.what();
    }
    return model;
}

} // namespace SagaEditor
