/// @file ExternalGateAdapter.cpp
/// @brief Generic external-tool gate execution for Forge.

#include "Forge/Adapters/ExternalGateAdapter.hpp"

#include "Forge/ProcessRunner.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace Forge::Adapters
{

namespace
{

bool ReadTextFile(const std::filesystem::path& path,
                  std::string&                 outText,
                  std::string&                 outError)
{
    std::ifstream in(path);
    if (!in)
    {
        outError = "diagnostics report was not written: " + path.generic_string();
        return false;
    }

    outText.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
    return true;
}

bool ReadJsonBoolean(const std::string& text,
                     const std::string& key,
                     bool&              outValue)
{
    const std::string token = "\"" + key + "\"";
    const std::size_t keyPos = text.find(token);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::size_t colon = text.find(':', keyPos + token.size());
    if (colon == std::string::npos)
    {
        return false;
    }

    std::size_t valuePos = colon + 1;
    while (valuePos < text.size() &&
           std::isspace(static_cast<unsigned char>(text[valuePos])))
    {
        ++valuePos;
    }

    if (text.compare(valuePos, 4, "true") == 0)
    {
        outValue = true;
        return true;
    }
    if (text.compare(valuePos, 5, "false") == 0)
    {
        outValue = false;
        return true;
    }

    return false;
}

void PrintCommand(const std::string& executable,
                  const std::vector<std::string>& args)
{
    std::cout << executable;
    for (const std::string& arg : args)
    {
        std::cout << " " << arg;
    }
    std::cout << "\n";
}

} // namespace

ExternalGateResult ExternalGateAdapter::Run(const ExternalGateOptions& options,
                                            std::string& outError)
{
    ExternalGateResult result;

    if (options.explain)
    {
        PrintCommand(options.toolExecutable, options.toolArguments);
        return result;
    }

    const std::filesystem::path diagnosticsPath = options.diagnosticsPath;
    if (!diagnosticsPath.parent_path().empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(diagnosticsPath.parent_path(), ec);
        if (ec)
        {
            outError = "could not create diagnostics directory '" +
                       diagnosticsPath.parent_path().generic_string() +
                       "': " + ec.message();
            result.toolExitCode = -1;
            return result;
        }
    }

    std::string spawnError;
    result.toolExitCode =
        ProcessRunner::Run(options.toolExecutable, options.toolArguments, &spawnError);
    if (result.toolExitCode < 0)
    {
        outError = spawnError;
        return result;
    }

    std::string diagnosticsText;
    if (!ReadTextFile(diagnosticsPath, diagnosticsText, outError))
    {
        return result;
    }
    result.diagnosticsRead = true;

    if (!ReadJsonBoolean(diagnosticsText,
                         options.blockingKey,
                         result.hasBlockingDiagnostics))
    {
        outError = "diagnostics report is missing boolean key '" +
                   options.blockingKey + "': " + diagnosticsPath.generic_string();
        result.diagnosticsRead = false;
    }

    return result;
}

} // namespace Forge::Adapters
