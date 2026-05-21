/// @file ToolGateTests.cpp
/// @brief Coverage for Forge generic external gate command behavior.

#include "Forge/ExitCode.h"
#include "Forge/ProcessRunner.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

bool Expect(bool condition, const char* label)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "[ToolGateTests] failed: " << label << "\n";
    return false;
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

int RunForge(const std::string& forge,
             const std::vector<std::string>& args)
{
    std::string error;
    const int rc = Forge::ProcessRunner::Run(forge, args, &error);
    if (rc < 0)
    {
        std::cerr << "[ToolGateTests] launch failed: " << error << "\n";
    }
    return rc;
}

std::vector<std::string> GateCommand(const std::string& fakeTool,
                                     const std::filesystem::path& diagnosticsPath)
{
    return {
        "gate",
        "run",
        "--name",
        "fixture",
        "--tool",
        fakeTool,
        "--diagnostics",
        diagnosticsPath.string(),
        "--"
    };
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "[ToolGateTests] expected forge and fake tool paths\n";
        return EXIT_FAILURE;
    }

    const std::string forge = argv[1];
    const std::string fakeTool = argv[2];
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "forge_tool_gate_tests";

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    bool ok = true;

    const std::filesystem::path passDiagnostics = root / "pass" / "diagnostics.json";
    const std::filesystem::path passRecord = root / "pass" / "record.txt";
    std::vector<std::string> passArgs = GateCommand(fakeTool, passDiagnostics);
    passArgs.insert(passArgs.end(),
                    {"--mode", "pass",
                     "--diagnostics", passDiagnostics.string(),
                     "--record", passRecord.string(),
                     "--sample", "value"});
    ok &= Expect(RunForge(forge, passArgs) == Forge::ToInt(Forge::ExitCode::Success),
                 "warning-free gate succeeds");
    ok &= Expect(std::filesystem::exists(passDiagnostics),
                 "successful gate writes diagnostics");
    ok &= Expect(ReadText(passRecord).find("--sample\nvalue\n") != std::string::npos,
                 "tool arguments after separator are passed through");

    const std::filesystem::path blockDiagnostics = root / "block" / "diagnostics.json";
    std::vector<std::string> blockArgs = GateCommand(fakeTool, blockDiagnostics);
    blockArgs.insert(blockArgs.end(),
                     {"--mode", "block",
                      "--diagnostics", blockDiagnostics.string()});
    ok &= Expect(RunForge(forge, blockArgs) ==
                     Forge::ToInt(Forge::ExitCode::ValidationFailure),
                 "blocking diagnostics fail validation");

    const std::filesystem::path missingDiagnostics = root / "missing" / "diagnostics.json";
    std::vector<std::string> missingArgs = GateCommand(fakeTool, missingDiagnostics);
    missingArgs.insert(missingArgs.end(), {"--mode", "missing"});
    ok &= Expect(RunForge(forge, missingArgs) ==
                     Forge::ToInt(Forge::ExitCode::ExecutionFailure),
                 "missing diagnostics fail execution");

    const std::filesystem::path customDiagnostics = root / "custom" / "diagnostics.json";
    std::vector<std::string> customArgs = {
        "gate",
        "run",
        "--name",
        "custom",
        "--tool",
        fakeTool,
        "--diagnostics",
        customDiagnostics.string(),
        "--expect-diagnostics-key",
        "customBlocking",
        "--",
        "--mode",
        "pass",
        "--key",
        "customBlocking",
        "--diagnostics",
        customDiagnostics.string()
    };
    ok &= Expect(RunForge(forge, customArgs) == Forge::ToInt(Forge::ExitCode::Success),
                 "custom diagnostics key is supported");

    const std::filesystem::path explainDiagnostics = root / "explain" / "diagnostics.json";
    std::vector<std::string> explainArgs = GateCommand(fakeTool, explainDiagnostics);
    explainArgs.insert(explainArgs.begin() + 2, "--explain");
    explainArgs.insert(explainArgs.end(),
                       {"--mode", "pass",
                        "--diagnostics", explainDiagnostics.string()});
    ok &= Expect(RunForge(forge, explainArgs) == Forge::ToInt(Forge::ExitCode::Success),
                 "explain succeeds");
    ok &= Expect(!std::filesystem::exists(explainDiagnostics),
                 "explain does not run external tool");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
