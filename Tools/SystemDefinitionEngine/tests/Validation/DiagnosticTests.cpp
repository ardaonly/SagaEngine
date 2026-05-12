/// @file DiagnosticTests.cpp
/// @brief Tests for the Diagnostic value type and factory methods.

#include "SDE/Validation/Diagnostic.h"

#include <gtest/gtest.h>

using namespace SDE;

TEST(DiagnosticTest, MakeErrorSetsSeverity)
{
    auto d = Diagnostic::MakeError({}, "SDE0001", "test message");
    EXPECT_EQ(d.severity, Severity::Error);
}

TEST(DiagnosticTest, MakeWarningSetsSeverity)
{
    auto d = Diagnostic::MakeWarning({}, "SDE0002", "test warning");
    EXPECT_EQ(d.severity, Severity::Warning);
}

TEST(DiagnosticTest, MakeFatalSetsSeverity)
{
    auto d = Diagnostic::MakeFatal({}, "SDE0003", "fatal error");
    EXPECT_EQ(d.severity, Severity::Fatal);
}

TEST(DiagnosticTest, MakeInfoSetsSeverity)
{
    auto d = Diagnostic::MakeInfo({}, "SDE0004", "informational");
    EXPECT_EQ(d.severity, Severity::Info);
}

TEST(DiagnosticTest, FactoryPreservesCode)
{
    auto d = Diagnostic::MakeError({}, "SDE_RANGE", "out of range");
    EXPECT_EQ(d.code, "SDE_RANGE");
}

TEST(DiagnosticTest, FactoryPreservesMessage)
{
    auto d = Diagnostic::MakeError({}, "X", "my message");
    EXPECT_EQ(d.message, "my message");
}

TEST(DiagnosticTest, SuggestionDefaultsToEmpty)
{
    auto d = Diagnostic::MakeError({}, "X", "msg");
    EXPECT_TRUE(d.suggestion.empty());
}

TEST(DiagnosticTest, SourceLocationDefaultsToZero)
{
    SourceLocation loc;
    EXPECT_EQ(loc.line, 0);
    EXPECT_EQ(loc.column, 0);
    EXPECT_TRUE(loc.file.empty());
}

TEST(DiagnosticTest, SourceLocationPreserved)
{
    SourceLocation loc{"myfile.json", 42, 7};
    auto d = Diagnostic::MakeError(loc, "X", "msg");
    EXPECT_EQ(d.location.file, "myfile.json");
    EXPECT_EQ(d.location.line, 42);
    EXPECT_EQ(d.location.column, 7);
}
