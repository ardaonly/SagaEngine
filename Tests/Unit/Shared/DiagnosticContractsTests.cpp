/// @file DiagnosticContractsTests.cpp
/// @brief Verifies shared diagnostic payload contracts.

#include <SagaShared/Diagnostics/DiagnosticCategory.hpp>
#include <SagaShared/Diagnostics/DiagnosticPayload.hpp>
#include <SagaShared/Diagnostics/DiagnosticSeverity.hpp>
#include <SagaShared/Diagnostics/DiagnosticSource.hpp>
#include <SagaShared/Diagnostics/DiagnosticSummary.hpp>

#include <gtest/gtest.h>

using SagaShared::Diagnostics::DiagnosticCategory;
using SagaShared::Diagnostics::DiagnosticPayload;
using SagaShared::Diagnostics::DiagnosticSeverity;
using SagaShared::Diagnostics::DiagnosticSeverityRank;
using SagaShared::Diagnostics::DiagnosticSource;
using SagaShared::Diagnostics::DiagnosticSummary;

TEST(SharedDiagnosticContractsTests, PayloadDefaultConstructs)
{
    const DiagnosticPayload payload;

    EXPECT_EQ(payload.severity, DiagnosticSeverity::Info);
    EXPECT_EQ(payload.category, DiagnosticCategory::Project);
    EXPECT_EQ(payload.source, DiagnosticSource::Product);
    EXPECT_TRUE(payload.code.value.empty());
    EXPECT_TRUE(payload.message.empty());
    EXPECT_EQ(payload.location.line, 0u);
    EXPECT_TRUE(payload.metadata.empty());
}

TEST(SharedDiagnosticContractsTests, ExposesStableSeverityCategoryAndSourceValues)
{
    EXPECT_EQ(DiagnosticSeverity::Trace, DiagnosticSeverity::Trace);
    EXPECT_EQ(DiagnosticSeverity::BuildBlocking, DiagnosticSeverity::BuildBlocking);
    EXPECT_EQ(DiagnosticSeverity::PublishBlocking, DiagnosticSeverity::PublishBlocking);
    EXPECT_EQ(DiagnosticSeverity::Security, DiagnosticSeverity::Security);
    EXPECT_LT(DiagnosticSeverityRank(DiagnosticSeverity::Warning),
              DiagnosticSeverityRank(DiagnosticSeverity::Error));
    EXPECT_LT(DiagnosticSeverityRank(DiagnosticSeverity::Fatal),
              DiagnosticSeverityRank(DiagnosticSeverity::BuildBlocking));

    EXPECT_EQ(DiagnosticCategory::Build, DiagnosticCategory::Build);
    EXPECT_EQ(DiagnosticCategory::Package, DiagnosticCategory::Package);
    EXPECT_EQ(DiagnosticCategory::Publish, DiagnosticCategory::Publish);
    EXPECT_EQ(DiagnosticCategory::DependencyBoundary, DiagnosticCategory::DependencyBoundary);

    EXPECT_EQ(DiagnosticSource::Forge, DiagnosticSource::Forge);
    EXPECT_EQ(DiagnosticSource::CI, DiagnosticSource::CI);
}

TEST(SharedDiagnosticContractsTests, PayloadCarriesCodeMessageAndLocation)
{
    DiagnosticPayload payload;
    payload.severity = DiagnosticSeverity::BuildBlocking;
    payload.category = DiagnosticCategory::Build;
    payload.source = DiagnosticSource::Forge;
    payload.code.value = "Build.Step.MissingOutput";
    payload.title = "Build output missing";
    payload.message = "The compile step did not produce the expected graph artifact.";
    payload.location.sourcePath = "Graphs/Combat.saga";
    payload.location.line = 12;
    payload.location.column = 5;
    payload.location.artifactId = "artifact://graphs/combat";
    payload.location.buildId = "build-42";
    payload.location.buildStepId = "compile-graphs";
    payload.suggestedAction = "Re-run graph compilation.";
    payload.metadata["profile"] = "debug-client";

    EXPECT_EQ(payload.severity, DiagnosticSeverity::BuildBlocking);
    EXPECT_EQ(payload.category, DiagnosticCategory::Build);
    EXPECT_EQ(payload.source, DiagnosticSource::Forge);
    EXPECT_EQ(payload.code.value, "Build.Step.MissingOutput");
    EXPECT_EQ(payload.message,
              "The compile step did not produce the expected graph artifact.");
    EXPECT_EQ(payload.location.sourcePath, "Graphs/Combat.saga");
    EXPECT_EQ(payload.location.line, 12u);
    EXPECT_EQ(payload.location.column, 5u);
    EXPECT_EQ(payload.location.artifactId, "artifact://graphs/combat");
    EXPECT_EQ(payload.location.buildId, "build-42");
    EXPECT_EQ(payload.location.buildStepId, "compile-graphs");
    EXPECT_EQ(payload.metadata.at("profile"), "debug-client");
}

TEST(SharedDiagnosticContractsTests, SummaryAggregatesSeverityCountsAndHighestSeverity)
{
    DiagnosticSummary summary;

    DiagnosticPayload warning;
    warning.severity = DiagnosticSeverity::Warning;
    DiagnosticPayload publishBlocking;
    publishBlocking.severity = DiagnosticSeverity::PublishBlocking;
    DiagnosticPayload security;
    security.severity = DiagnosticSeverity::Security;

    summary.Add(warning);
    summary.Add(publishBlocking);
    summary.Add(security);
    summary.Add(DiagnosticSeverity::Warning);

    EXPECT_EQ(summary.totalCount, 4u);
    EXPECT_EQ(summary.Count(DiagnosticSeverity::Warning), 2u);
    EXPECT_EQ(summary.Count(DiagnosticSeverity::PublishBlocking), 1u);
    EXPECT_EQ(summary.Count(DiagnosticSeverity::Security), 1u);
    EXPECT_EQ(summary.highestSeverity, DiagnosticSeverity::Security);
}
