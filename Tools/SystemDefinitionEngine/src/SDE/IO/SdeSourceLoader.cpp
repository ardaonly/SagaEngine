/// @file SdeSourceLoader.cpp
/// @brief Internal native .sde source parser/lowering implementation.

#include "SDE/IO/SdeSourceLoader.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace SDE::Internal
{
namespace
{

enum class TokenKind
{
    Identifier,
    String,
    Integer,
    Boolean,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    LAngle,
    RAngle,
    Comma,
    Equal,
    End,
    Invalid,
};

struct Token
{
    TokenKind      kind = TokenKind::Invalid;
    std::string    text;
    SourceLocation location;
};

Diagnostic MakeDiag(SourceLocation loc,
                    Severity severity,
                    DiagnosticCategory category,
                    std::string code,
                    std::string message)
{
    Diagnostic d = severity == Severity::Fatal
        ? Diagnostic::MakeFatal(std::move(loc), std::move(code), std::move(message))
        : Diagnostic::MakeError(std::move(loc), std::move(code), std::move(message));
    d.category = category;
    return d;
}

class Lexer
{
public:
    Lexer(std::string text, std::string file)
        : m_text(std::move(text))
        , m_file(std::move(file))
    {
    }

    [[nodiscard]] std::vector<Token> Tokenize(std::vector<Diagnostic>& diagnostics)
    {
        std::vector<Token> tokens;
        while (!AtEnd())
        {
            SkipTrivia();
            if (AtEnd())
                break;

            const SourceLocation loc{m_file, m_line, m_column};
            const char c = Peek();
            if (IsIdentStart(c))
            {
                tokens.push_back(ReadIdentifier(loc));
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                tokens.push_back(ReadInteger(loc));
                continue;
            }
            if (c == '"')
            {
                tokens.push_back(ReadString(loc, diagnostics));
                continue;
            }

            Advance();
            switch (c)
            {
                case '{': tokens.push_back({TokenKind::LBrace, "{", loc}); break;
                case '}': tokens.push_back({TokenKind::RBrace, "}", loc}); break;
                case '[': tokens.push_back({TokenKind::LBracket, "[", loc}); break;
                case ']': tokens.push_back({TokenKind::RBracket, "]", loc}); break;
                case '<': tokens.push_back({TokenKind::LAngle, "<", loc}); break;
                case '>': tokens.push_back({TokenKind::RAngle, ">", loc}); break;
                case ',': tokens.push_back({TokenKind::Comma, ",", loc}); break;
                case '=': tokens.push_back({TokenKind::Equal, "=", loc}); break;
                default:
                    diagnostics.push_back(MakeDiag(
                        loc, Severity::Error, DiagnosticCategory::Parse, "SDE_UNEXPECTED_CHAR",
                        std::string("Unexpected character in .sde source: '") + c + "'."));
                    tokens.push_back({TokenKind::Invalid, std::string(1, c), loc});
                    break;
            }
        }
        tokens.push_back({TokenKind::End, {}, {m_file, m_line, m_column}});
        return tokens;
    }

private:
    std::string m_text;
    std::string m_file;
    std::size_t m_pos = 0;
    int m_line = 1;
    int m_column = 1;

    [[nodiscard]] bool AtEnd() const noexcept { return m_pos >= m_text.size(); }
    [[nodiscard]] char Peek() const noexcept { return AtEnd() ? '\0' : m_text[m_pos]; }
    [[nodiscard]] char PeekNext() const noexcept
    {
        return m_pos + 1 >= m_text.size() ? '\0' : m_text[m_pos + 1];
    }

    char Advance()
    {
        const char c = m_text[m_pos++];
        if (c == '\n')
        {
            ++m_line;
            m_column = 1;
        }
        else
        {
            ++m_column;
        }
        return c;
    }

    void SkipTrivia()
    {
        bool progressed = true;
        while (progressed && !AtEnd())
        {
            progressed = false;
            while (!AtEnd() && std::isspace(static_cast<unsigned char>(Peek())))
            {
                Advance();
                progressed = true;
            }
            if (!AtEnd() && Peek() == '#')
            {
                while (!AtEnd() && Peek() != '\n')
                    Advance();
                progressed = true;
            }
            if (!AtEnd() && Peek() == '/' && PeekNext() == '/')
            {
                while (!AtEnd() && Peek() != '\n')
                    Advance();
                progressed = true;
            }
        }
    }

    [[nodiscard]] static bool IsIdentStart(char c) noexcept
    {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    [[nodiscard]] static bool IsIdentBody(char c) noexcept
    {
        return std::isalnum(static_cast<unsigned char>(c)) ||
               c == '_' || c == '.' || c == '-' || c == ':';
    }

    Token ReadIdentifier(SourceLocation loc)
    {
        std::string text;
        while (!AtEnd() && IsIdentBody(Peek()))
            text.push_back(Advance());
        if (text == "true" || text == "false")
            return {TokenKind::Boolean, text, loc};
        return {TokenKind::Identifier, text, loc};
    }

    Token ReadInteger(SourceLocation loc)
    {
        std::string text;
        while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
            text.push_back(Advance());
        return {TokenKind::Integer, text, loc};
    }

    Token ReadString(SourceLocation loc, std::vector<Diagnostic>& diagnostics)
    {
        Advance(); // opening quote
        std::string text;
        while (!AtEnd() && Peek() != '"')
        {
            if (Peek() == '\\')
            {
                Advance();
                if (AtEnd())
                    break;
                const char escaped = Advance();
                switch (escaped)
                {
                    case 'n': text.push_back('\n'); break;
                    case 't': text.push_back('\t'); break;
                    case '"': text.push_back('"'); break;
                    case '\\': text.push_back('\\'); break;
                    default: text.push_back(escaped); break;
                }
                continue;
            }
            text.push_back(Advance());
        }
        if (AtEnd())
        {
            diagnostics.push_back(MakeDiag(
                loc, Severity::Error, DiagnosticCategory::Parse, "SDE_UNTERMINATED_STRING",
                "Unterminated string literal."));
            return {TokenKind::String, text, loc};
        }
        Advance(); // closing quote
        return {TokenKind::String, text, loc};
    }
};

TypeNodeId ParseTypeString(const std::string& s,
                           TypeRegistry& types,
                           std::vector<Diagnostic>& out,
                           const SourceLocation& loc)
{
    if (s == "Number")  return types.Primitive(TypeKind::Number);
    if (s == "Integer") return types.Primitive(TypeKind::Integer);
    if (s == "Text")    return types.Primitive(TypeKind::Text);
    if (s == "Boolean") return types.Primitive(TypeKind::Boolean);
    if (s == "Color")   return types.Primitive(TypeKind::Color);
    if (s.size() >= 5 && s.substr(0, 4) == "Ref<" && s.back() == '>')
        return types.Ref(s.substr(4, s.size() - 5));
    if (s.size() >= 7 && s.substr(0, 5) == "Enum<" && s.back() == '>')
        return types.Enum(s.substr(5, s.size() - 6));
    if (s.size() >= 7 && s.substr(0, 5) == "List<" && s.back() == '>')
    {
        const TypeNodeId inner = ParseTypeString(s.substr(5, s.size() - 6), types, out, loc);
        return inner == kInvalidTypeNodeId ? kInvalidTypeNodeId : types.List(inner);
    }
    out.push_back(MakeDiag(
        loc, Severity::Error, DiagnosticCategory::Schema, "SDE_UNKNOWN_TYPE",
        "Unknown type string: '" + s + "'."));
    return kInvalidTypeNodeId;
}

class Parser
{
public:
    Parser(std::vector<Token> tokens,
           std::filesystem::path path,
           TypeRegistry& types,
           std::vector<Diagnostic>& diagnostics)
        : m_tokens(std::move(tokens))
        , m_path(std::move(path))
        , m_types(types)
        , m_diagnostics(diagnostics)
    {
    }

    [[nodiscard]] SdeSourceLoadResult Parse()
    {
        SdeSourceLoadResult result;
        SdeSourceFileSummary file;
        file.path = m_path;

        if (!ExpectKeyword("sde", "SDE source file must start with 'sde version <n>'."))
            return result;
        if (!ExpectKeyword("version", "Expected 'version' after 'sde'."))
            return result;
        const Token version = Consume(TokenKind::Integer, "Expected SDE language version number.");
        if (!version.text.empty())
            file.languageVersion = std::stoi(version.text);
        if (file.languageVersion != 1)
        {
            m_diagnostics.push_back(MakeDiag(
                version.location, Severity::Error, DiagnosticCategory::Migration,
                "SDE_UNSUPPORTED_LANGUAGE_VERSION",
                "Unsupported SDE language version " + version.text + "."));
        }

        if (MatchKeyword("package"))
        {
            const Token package = Consume(TokenKind::Identifier, "Expected package name.");
            file.packageName = package.text;
        }
        else
        {
            m_diagnostics.push_back(MakeDiag(
                Peek().location, Severity::Error, DiagnosticCategory::Parse,
                "SDE_MISSING_PACKAGE", "SDE source file must declare a package."));
        }

        while (MatchKeyword("import"))
        {
            const Token import = Consume(TokenKind::Identifier, "Expected imported package name.");
            m_imports.push_back(import.text);
        }

        while (!Check(TokenKind::End))
        {
            if (MatchKeyword("model"))
            {
                result.definitions.push_back(ParseModel());
                continue;
            }
            if (MatchKeyword("instance"))
            {
                result.instances.push_back(ParseInstance());
                continue;
            }
            m_diagnostics.push_back(MakeDiag(
                Peek().location, Severity::Error, DiagnosticCategory::Parse,
                "SDE_EXPECTED_DECLARATION",
                "Expected model or instance declaration."));
            Advance();
        }

        for (ModelDefinition& def : result.definitions)
            def.imports = m_imports;
        result.files.push_back(std::move(file));
        return result;
    }

private:
    std::vector<Token> m_tokens;
    std::filesystem::path m_path;
    TypeRegistry& m_types;
    std::vector<Diagnostic>& m_diagnostics;
    std::vector<std::string> m_imports;
    std::size_t m_current = 0;

    [[nodiscard]] const Token& Peek() const { return m_tokens[m_current]; }
    [[nodiscard]] const Token& Previous() const { return m_tokens[m_current - 1]; }
    [[nodiscard]] bool Check(TokenKind kind) const { return Peek().kind == kind; }
    [[nodiscard]] bool AtEnd() const { return Check(TokenKind::End); }

    const Token& Advance()
    {
        if (!AtEnd())
            ++m_current;
        return Previous();
    }

    [[nodiscard]] bool Match(TokenKind kind)
    {
        if (!Check(kind))
            return false;
        Advance();
        return true;
    }

    [[nodiscard]] bool MatchKeyword(const std::string& text)
    {
        if (Peek().kind != TokenKind::Identifier || Peek().text != text)
            return false;
        Advance();
        return true;
    }

    bool ExpectKeyword(const std::string& text, const std::string& message)
    {
        if (MatchKeyword(text))
            return true;
        m_diagnostics.push_back(MakeDiag(
            Peek().location, Severity::Error, DiagnosticCategory::Parse,
            "SDE_EXPECTED_KEYWORD", message));
        return false;
    }

    Token Consume(TokenKind kind, const std::string& message)
    {
        if (Check(kind))
            return Advance();
        m_diagnostics.push_back(MakeDiag(
            Peek().location, Severity::Error, DiagnosticCategory::Parse,
            "SDE_EXPECTED_TOKEN", message));
        return {};
    }

    std::string ParseTypeName()
    {
        const Token base = Consume(TokenKind::Identifier, "Expected field type.");
        std::string text = base.text;
        if (Match(TokenKind::LAngle))
        {
            text += '<';
            text += ParseTypeName();
            Consume(TokenKind::RAngle, "Expected '>' after type parameter.");
            text += '>';
        }
        return text;
    }

    ModelDefinition ParseModel()
    {
        ModelDefinition def;
        const Token name = Consume(TokenKind::Identifier, "Expected model name.");
        def.id = name.text;
        def.displayName = name.text;
        ExpectKeyword("version", "Expected model version declaration.");
        const Token version = Consume(TokenKind::Integer, "Expected model version number.");
        if (!version.text.empty())
            def.schemaVersion = std::stoi(version.text);
        if (def.schemaVersion != 1)
        {
            m_diagnostics.push_back(MakeDiag(
                version.location, Severity::Error, DiagnosticCategory::Migration,
                "SDE_UNSUPPORTED_MODEL_VERSION",
                "Model '" + def.id + "' uses unsupported version " + version.text + "."));
        }
        Consume(TokenKind::LBrace, "Expected '{' before model body.");
        while (!Check(TokenKind::RBrace) && !AtEnd())
        {
            if (!ExpectKeyword("field", "Expected field declaration."))
            {
                Advance();
                continue;
            }
            FieldDefinition field;
            const Token fieldId = Consume(TokenKind::Identifier, "Expected field name.");
            field.id = fieldId.text;
            field.displayName = field.id;
            const std::string typeName = ParseTypeName();
            field.type = ParseTypeString(typeName, m_types, m_diagnostics, fieldId.location);
            if (MatchKeyword("required"))
                field.presence = FieldPresence::Required;
            else if (MatchKeyword("optional"))
                field.presence = FieldPresence::Optional;
            else
                m_diagnostics.push_back(MakeDiag(
                    Peek().location, Severity::Error, DiagnosticCategory::Parse,
                    "SDE_EXPECTED_FIELD_PRESENCE",
                    "Expected field presence: required or optional."));
            def.fields.push_back(std::move(field));
        }
        Consume(TokenKind::RBrace, "Expected '}' after model body.");
        return def;
    }

    ModelInstance ParseInstance()
    {
        ModelInstance inst;
        const Token model = Consume(TokenKind::Identifier, "Expected instance model name.");
        const Token id = Consume(TokenKind::Identifier, "Expected instance id.");
        inst.modelId = model.text;
        inst.instanceId = id.text;
        inst.sourceFile = m_path.string();
        inst.origin = id.location;
        Consume(TokenKind::LBrace, "Expected '{' before instance body.");
        while (!Check(TokenKind::RBrace) && !AtEnd())
        {
            const Token field = Consume(TokenKind::Identifier, "Expected instance field name.");
            Consume(TokenKind::Equal, "Expected '=' after instance field name.");
            inst.fields[field.text] = ParseValue();
        }
        Consume(TokenKind::RBrace, "Expected '}' after instance body.");
        return inst;
    }

    RawValue ParseValue()
    {
        RawValue value;
        value.location = Peek().location;
        if (Match(TokenKind::String))
        {
            value.data = RawText{Previous().text};
            return value;
        }
        if (Match(TokenKind::Integer))
        {
            value.data = static_cast<RawInteger>(std::stoll(Previous().text));
            return value;
        }
        if (Match(TokenKind::Boolean))
        {
            value.data = RawBool{Previous().text == "true"};
            return value;
        }
        if (Match(TokenKind::LBracket))
        {
            RawArray array;
            while (!Check(TokenKind::RBracket) && !AtEnd())
            {
                array.elements.push_back(ParseValue());
                if (!Match(TokenKind::Comma))
                    break;
            }
            Consume(TokenKind::RBracket, "Expected ']' after list literal.");
            value.data = std::move(array);
            return value;
        }
        if (Match(TokenKind::Identifier))
        {
            value.data = RawText{Previous().text};
            return value;
        }
        m_diagnostics.push_back(MakeDiag(
            Peek().location, Severity::Error, DiagnosticCategory::Parse,
            "SDE_EXPECTED_VALUE", "Expected literal value."));
        Advance();
        value.data = RawNull{};
        return value;
    }
};

} // namespace

SdeSourceLoadResult LoadSdeSourceFile(const std::filesystem::path& path,
                                      TypeRegistry& types,
                                      std::vector<Diagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        diagnostics.push_back(MakeDiag(
            {path.string(), 0, 0}, Severity::Fatal, DiagnosticCategory::IO,
            "SDE_IO_ERROR", "Cannot open .sde source file: '" + path.string() + "'."));
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    Lexer lexer(buffer.str(), path.string());
    Parser parser(lexer.Tokenize(diagnostics), path, types, diagnostics);
    return parser.Parse();
}

} // namespace SDE::Internal
