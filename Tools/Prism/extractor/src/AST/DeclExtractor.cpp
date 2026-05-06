/// @file AST/DeclExtractor.cpp
/// @brief DeclExtractor implementation — all Clang NamedDecl introspection lives here.

#include "DeclExtractor.h"

#include "../Support/CommentParser.h"
#include "../Support/IdGen.h"
#include "../Support/PathUtils.h"

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>

#include <filesystem>
#include <sstream>

namespace prism::ast {

// ─── Construction ─────────────────────────────────────────────────────────────

DeclExtractor::DeclExtractor(clang::ASTContext& ctx, const std::string& repo_root)
    : m_ctx(ctx)
    , m_sm(ctx.getSourceManager())
    , m_repo_root(repo_root)
{}

// ─── ShouldIndex Guard ────────────────────────────────────────────────────────

bool DeclExtractor::ShouldIndex(const clang::NamedDecl* decl) const
{
    if (!decl || decl->isImplicit())
        return false;

    // Anonymous declarations (unnamed structs, unnamed unions, etc.)
    if (!decl->getIdentifier() && !decl->getDeclName())
        return false;

    const clang::SourceLocation loc = decl->getLocation();
    if (!loc.isValid())
        return false;

    // Reject system headers unconditionally.
    if (m_sm.isInSystemHeader(m_sm.getExpansionLoc(loc)))
        return false;

    // Reject locations that don't resolve to a real file.
    const auto fe = m_sm.getFileEntryRefForID(
        m_sm.getFileID(m_sm.getExpansionLoc(loc)));
    if (!fe)
        return false;

    const std::string abs_path(fe->getName());
    if (support::IsSystemPath(abs_path))
        return false;

    return true;
}

// ─── Public API ───────────────────────────────────────────────────────────────

bool DeclExtractor::Extract(const clang::NamedDecl* decl,
                             SymbolRecord&           out) const
{
    if (!ShouldIndex(decl))
        return false;

    const std::string usr = GetUSR(decl);
    const SourceLocation loc = MakeLocation(decl->getLocation());

    out.id             = support::MakeSymbolId(usr, loc.file, loc.line, GetKindLabel(decl));
    out.usr            = usr;
    out.name           = decl->getNameAsString();
    out.qualified_name = GetQualifiedName(decl);
    out.display_name   = decl->getDeclName().getAsString();
    out.kind           = GetKindLabel(decl);
    out.access         = GetAccessStr(decl->getAccess());
    out.location       = loc;
    out.extent         = MakeExtent(decl->getSourceRange());

    // Definition flag — works for most DeclKinds.
    if (const auto* rd = clang::dyn_cast<clang::RecordDecl>(decl))
        out.is_definition = rd->isThisDeclarationADefinition();
    else if (const auto* fd = clang::dyn_cast<clang::FunctionDecl>(decl))
        out.is_definition = fd->isThisDeclarationADefinition();
    else if (const auto* vd = clang::dyn_cast<clang::VarDecl>(decl))
        out.is_definition = (vd->isThisDeclarationADefinition() == clang::VarDecl::Definition);
    else
        out.is_definition = false;

    // Comments.
    out.raw_comment = GetRawComment(decl);
    out.brief       = support::ExtractBrief(out.raw_comment);
    // Type / signature.
    if (const auto* fd = clang::dyn_cast<clang::FunctionDecl>(decl))
    {
        out.result_type = fd->getReturnType().getAsString();
        out.signature   = GetSignature(fd);
    }
    else if (const auto* vd = clang::dyn_cast<clang::ValueDecl>(decl))
    {
        out.type_spelling = vd->getType().getAsString();
    }

    return true;
}

// ─── Metadata Helpers ─────────────────────────────────────────────────────────

std::string DeclExtractor::GetUSR(const clang::NamedDecl* decl) const
{
    llvm::SmallString<256> buf;
    if (clang::index::generateUSRForDecl(decl, buf))
        return {};
    return buf.str().str();
}

std::string DeclExtractor::GetQualifiedName(const clang::NamedDecl* decl) const
{
    std::string qn;
    llvm::raw_string_ostream os(qn);
    decl->printQualifiedName(os);
    return os.str();
}

std::string DeclExtractor::GetKindLabel(const clang::Decl* decl) const
{
    return decl->getDeclKindName();
}

std::string DeclExtractor::GetAccessStr(clang::AccessSpecifier as) const
{
    switch (as)
    {
    case clang::AS_public:    return "public";
    case clang::AS_protected: return "protected";
    case clang::AS_private:   return "private";
    default:                  return "none";
    }
}

std::string DeclExtractor::GetRawComment(const clang::Decl* decl) const
{
    const clang::RawComment* rc = m_ctx.getRawCommentForDeclNoCache(decl);
    return rc ? rc->getRawText(m_sm).str() : std::string{};
}

std::string DeclExtractor::GetSignature(const clang::FunctionDecl* fd) const
{
    // Use Clang's pretty printer for a canonical, human-readable form.
    std::string sig;
    llvm::raw_string_ostream os(sig);
    clang::PrintingPolicy pp(m_ctx.getLangOpts());
    pp.TerseOutput    = true;
    pp.FullyQualifiedName = false;
    fd->print(os, pp);
    return sig;
}

SourceLocation DeclExtractor::MakeLocation(clang::SourceLocation loc) const
{
    if (!loc.isValid()) return {};

    const clang::SourceLocation exp = m_sm.getExpansionLoc(loc);

    const auto fe = m_sm.getFileEntryRefForID(m_sm.getFileID(exp));
    const std::string abs = fe ? std::string(fe->getName()) : std::string{};

    return {
        support::MakeRelativePosix(abs, m_repo_root),
        m_sm.getExpansionLineNumber(exp),
        m_sm.getExpansionColumnNumber(exp)
    };
}

SourceExtent DeclExtractor::MakeExtent(clang::SourceRange range) const
{
    if (!range.isValid()) return {};

    const clang::SourceLocation sb = m_sm.getExpansionLoc(range.getBegin());
    const clang::SourceLocation se = m_sm.getExpansionLoc(range.getEnd());

    return {
        m_sm.getExpansionLineNumber(sb),
        m_sm.getExpansionColumnNumber(sb),
        m_sm.getExpansionLineNumber(se),
        m_sm.getExpansionColumnNumber(se)
    };
}

} // namespace prism::ast
