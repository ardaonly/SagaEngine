/// @file FakeGateTool.cpp
/// @brief Test-only external diagnostics tool used by Forge gate tests.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool TakeFlag(std::vector<std::string>& args,
              const std::string&        name,
              std::string&              outValue)
{
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        if (*it == "--" + name && it + 1 != args.end())
        {
            outValue = *(it + 1);
            args.erase(it, it + 2);
            return true;
        }
    }
    return false;
}

void WriteDiagnostics(const std::filesystem::path& path,
                      const std::string&           key,
                      const bool                   blocked)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::trunc);
    out << "{\n"
        << "  \"summary\": {\n"
        << "    \"" << key << "\": " << (blocked ? "true" : "false") << "\n"
        << "  }\n"
        << "}\n";
}

void WriteRecord(const std::filesystem::path& path,
                 const std::vector<std::string>& args)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::trunc);
    for (const std::string& arg : args)
    {
        out << arg << "\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    for (int index = 1; index < argc; ++index)
    {
        args.emplace_back(argv[index]);
    }

    std::string mode = "pass";
    std::string diagnosticsPath;
    std::string recordPath;
    std::string key = "hasBlockingDiagnostics";
    std::string exitCodeText;

    TakeFlag(args, "mode", mode);
    TakeFlag(args, "diagnostics", diagnosticsPath);
    TakeFlag(args, "record", recordPath);
    TakeFlag(args, "key", key);
    TakeFlag(args, "exit-code", exitCodeText);

    if (!recordPath.empty())
    {
        WriteRecord(recordPath, args);
    }

    if (mode != "missing")
    {
        if (diagnosticsPath.empty())
        {
            std::cerr << "fake gate requires --diagnostics\n";
            return 2;
        }

        WriteDiagnostics(diagnosticsPath, key, mode == "block");
    }

    if (!exitCodeText.empty())
    {
        return std::stoi(exitCodeText);
    }

    return 0;
}
