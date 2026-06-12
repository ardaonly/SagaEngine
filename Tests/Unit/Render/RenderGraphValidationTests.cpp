#include <gtest/gtest.h>

#include "SagaEngine/Render/RenderGraph/RenderGraphFrameExecutor.h"
#include "SagaEngine/Render/RenderGraph/RenderGraph.h"
#include "SagaEngine/RHI/IRHI.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{

namespace RG = SagaEngine::Render::RG;
namespace RHI = SagaEngine::RHI;

class FakeRHI final : public RHI::IRHI
{
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}

    RHI::BufferHandle CreateBuffer(const RHI::BufferDesc&) override
    {
        return RHI::BufferHandle::kInvalid;
    }

    RHI::ShaderHandle CreateShader(const RHI::ShaderDesc&) override
    {
        return RHI::ShaderHandle::kInvalid;
    }

    RHI::PipelineHandle CreateGraphicsPipeline(
        const RHI::GraphicsPipelineDesc&) override
    {
        return RHI::PipelineHandle::kInvalid;
    }

    void DestroyBuffer(RHI::BufferHandle) override {}
    void DestroyShader(RHI::ShaderHandle) override {}
    void DestroyPipeline(RHI::PipelineHandle) override {}

    void* MapBuffer(RHI::BufferHandle) override { return nullptr; }
    void UnmapBuffer(RHI::BufferHandle) override {}

    void BeginFrame() override {}
    void EndFrame() override {}
};

class RecordingRHI final : public RHI::IRHI
{
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}

    RHI::BufferHandle CreateBuffer(const RHI::BufferDesc&) override
    {
        return RHI::BufferHandle::kInvalid;
    }

    RHI::ShaderHandle CreateShader(const RHI::ShaderDesc&) override
    {
        return RHI::ShaderHandle::kInvalid;
    }

    RHI::PipelineHandle CreateGraphicsPipeline(
        const RHI::GraphicsPipelineDesc&) override
    {
        return RHI::PipelineHandle::kInvalid;
    }

    void DestroyBuffer(RHI::BufferHandle) override {}
    void DestroyShader(RHI::ShaderHandle) override {}
    void DestroyPipeline(RHI::PipelineHandle) override {}

    void* MapBuffer(RHI::BufferHandle) override { return nullptr; }
    void UnmapBuffer(RHI::BufferHandle) override {}

    void BeginFrame() override
    {
        ++beginFrameCalls;
        events.push_back("begin");
    }

    void EndFrame() override
    {
        ++endFrameCalls;
        events.push_back("end");
    }

    std::uint32_t beginFrameCalls = 0;
    std::uint32_t endFrameCalls = 0;
    std::vector<std::string> events;
};

RG::RGTextureDesc TextureDesc(const char* name = "color")
{
    RG::RGTextureDesc desc;
    desc.debugName = name;
    desc.width = 4;
    desc.height = 4;
    return desc;
}

RG::RGBufferDesc BufferDesc(const char* name = "constants")
{
    RG::RGBufferDesc desc;
    desc.debugName = name;
    desc.sizeBytes = 64;
    return desc;
}

RG::RGResourceUsage Read(RG::RGResourceId id)
{
    return {id, RG::RGAccess::Read};
}

RG::RGResourceUsage Write(RG::RGResourceId id)
{
    return {id, RG::RGAccess::Write};
}

bool HasDiagnostic(const RG::RGCompileSnapshot& snapshot,
                   RG::RGDiagnosticCode code)
{
    return std::any_of(
        snapshot.diagnostics.begin(),
        snapshot.diagnostics.end(),
        [code](const RG::RGDiagnostic& diagnostic)
        {
            return diagnostic.code == code;
        });
}

void ExpectTextOrder(const std::string& text,
                     const std::string& first,
                     const std::string& second)
{
    const auto firstIndex = text.find(first);
    const auto secondIndex = text.find(second);
    ASSERT_NE(firstIndex, std::string::npos);
    ASSERT_NE(secondIndex, std::string::npos);
    EXPECT_LT(firstIndex, secondIndex);
}

} // namespace

TEST(RenderGraphValidation, DeterministicDumpIsStableAcrossCompiles)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    ASSERT_TRUE(graph.Compile());
    const auto firstDump = graph.DumpLastCompile();

    ASSERT_TRUE(graph.Compile());
    const auto secondDump = graph.DumpLastCompile();

    EXPECT_EQ(firstDump, secondDump);
    EXPECT_NE(firstDump.find("RenderGraphDump v1"), std::string::npos);
    EXPECT_NE(firstDump.find("\"write\""), std::string::npos);
}

TEST(RenderGraphValidation, EmptyGraphDumpIsStableAndStructured)
{
    RG::RenderGraph graph;

    ASSERT_TRUE(graph.Compile());
    const auto firstDump = graph.DumpLastCompile();

    ASSERT_TRUE(graph.Compile());
    const auto secondDump = graph.DumpLastCompile();

    EXPECT_EQ(firstDump, secondDump);
    EXPECT_NE(firstDump.find("RenderGraphDump v1"), std::string::npos);
    EXPECT_NE(firstDump.find("valid: true"), std::string::npos);
    EXPECT_NE(firstDump.find("dirty: false"), std::string::npos);
    EXPECT_NE(firstDump.find("passCount: 0"), std::string::npos);
    EXPECT_NE(firstDump.find("resourceCount: 0"), std::string::npos);
    EXPECT_NE(firstDump.find("compiledPassCount: 0"), std::string::npos);
    EXPECT_NE(firstDump.find("resources:\npasses:\ncompiled:\n"),
              std::string::npos);
    EXPECT_NE(firstDump.find("diagnostics:\n"), std::string::npos);
    EXPECT_TRUE(graph.GetLastCompileSnapshot().diagnostics.empty());
}

TEST(RenderGraphValidation, DumpIncludesResourcesPassesAndCompiledOrder)
{
    RG::RenderGraph graph;
    const auto constants = graph.AddBuffer(BufferDesc());
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write constants",
                  {},
                  {Write(constants)},
                  [](RHI::IRHI&) {});
    graph.AddPass("write color",
                  {Read(constants)},
                  {Write(color)},
                  [](RHI::IRHI&) {});
    graph.AddPass("read color",
                  {Read(color)},
                  {},
                  [](RHI::IRHI&) {});

    ASSERT_TRUE(graph.Compile());
    const auto dump = graph.DumpLastCompile();

    EXPECT_NE(dump.find("passCount: 3"), std::string::npos);
    EXPECT_NE(dump.find("resourceCount: 2"), std::string::npos);
    EXPECT_NE(dump.find("compiledPassCount: 3"), std::string::npos);
    EXPECT_NE(dump.find("1 buffer \"constants\" sizeBytes=64"),
              std::string::npos);
    EXPECT_NE(dump.find("2 texture \"color\" width=4 height=4"),
              std::string::npos);
    EXPECT_NE(dump.find("format="), std::string::npos);
    EXPECT_NE(dump.find("lifetime="), std::string::npos);
    EXPECT_NE(dump.find("[0] \"write constants\" inputs=[] outputs=[1]"),
              std::string::npos);
    EXPECT_NE(dump.find("[1] \"write color\" inputs=[1] outputs=[2]"),
              std::string::npos);
    EXPECT_NE(dump.find("[2] \"read color\" inputs=[2] outputs=[]"),
              std::string::npos);
    EXPECT_NE(
        dump.find("compiled:\n"
                  "  [0] \"write constants\"\n"
                  "  [1] \"write color\"\n"
                  "  [2] \"read color\"\n"),
        std::string::npos);
}

TEST(RenderGraphValidation, SuccessfulCompileSnapshotIncludesCounts)
{
    RG::RenderGraph graph;
    const auto constants = graph.AddBuffer(BufferDesc());
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write constants",
                  {},
                  {Write(constants)},
                  [](RHI::IRHI&) {});
    graph.AddPass("write color",
                  {Read(constants)},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    ASSERT_TRUE(graph.Compile());
    const auto& snapshot = graph.GetLastCompileSnapshot();
    EXPECT_TRUE(snapshot.valid);
    EXPECT_EQ(snapshot.passCount, 2u);
    EXPECT_EQ(snapshot.resourceCount, 2u);
    EXPECT_EQ(snapshot.compiledPassCount, 2u);
    EXPECT_EQ(graph.PassCount(), 2u);
    EXPECT_EQ(graph.ResourceCount(), 2u);
    EXPECT_TRUE(graph.HasCompiledState());
    EXPECT_TRUE(graph.IsCompiledStateValid());
    EXPECT_FALSE(graph.IsCompiledStateDirty());
}

TEST(RenderGraphValidation, FailedCompileSnapshotIncludesCounts)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("read",
                  {Read(color)},
                  {},
                  [](RHI::IRHI&) {});

    ASSERT_FALSE(graph.Compile());
    const auto& snapshot = graph.GetLastCompileSnapshot();
    EXPECT_FALSE(snapshot.valid);
    EXPECT_EQ(snapshot.passCount, 1u);
    EXPECT_EQ(snapshot.resourceCount, 1u);
    EXPECT_EQ(snapshot.compiledPassCount, 0u);
    EXPECT_TRUE(graph.HasCompiledState());
    EXPECT_FALSE(graph.IsCompiledStateValid());
    EXPECT_FALSE(graph.IsCompiledStateDirty());
}

TEST(RenderGraphValidation, EmptyTextureNameDiagnostic)
{
    RG::RenderGraph graph;
    auto desc = TextureDesc("");
    const auto color = graph.AddTexture(desc);

    graph.AddPass("write",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::EmptyResourceName));
}

TEST(RenderGraphValidation, InvalidTextureSizeDiagnostic)
{
    RG::RenderGraph graph;
    auto desc = TextureDesc();
    desc.width = 0u;
    const auto color = graph.AddTexture(desc);

    graph.AddPass("write",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::InvalidTextureDesc));
}

TEST(RenderGraphValidation, EmptyBufferNameDiagnostic)
{
    RG::RenderGraph graph;
    auto desc = BufferDesc("");
    const auto constants = graph.AddBuffer(desc);

    graph.AddPass("write",
                  {},
                  {Write(constants)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::EmptyResourceName));
}

TEST(RenderGraphValidation, InvalidBufferSizeDiagnostic)
{
    RG::RenderGraph graph;
    auto desc = BufferDesc();
    desc.sizeBytes = 0u;
    const auto constants = graph.AddBuffer(desc);

    graph.AddPass("write",
                  {},
                  {Write(constants)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::InvalidBufferDesc));
}

TEST(RenderGraphValidation, DuplicatePassNameDiagnostic)
{
    RG::RenderGraph graph;

    graph.AddPass("utility", [](RHI::IRHI&) {});
    graph.AddPass("utility", [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::DuplicatePassName));
}

TEST(RenderGraphValidation, DuplicateInputResourceDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("read",
                  {Read(color), Read(color)},
                  {},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(
        HasDiagnostic(
            graph.GetLastCompileSnapshot(),
            RG::RGDiagnosticCode::DuplicatePassResourceUsage));
}

TEST(RenderGraphValidation, DuplicateOutputResourceDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write",
                  {},
                  {Write(color), Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(
        HasDiagnostic(
            graph.GetLastCompileSnapshot(),
            RG::RGDiagnosticCode::DuplicatePassResourceUsage));
}

TEST(RenderGraphValidation, SamePassReadWriteResourceDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("read-write",
                  {Read(color)},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(
        HasDiagnostic(
            graph.GetLastCompileSnapshot(),
            RG::RGDiagnosticCode::PassReadsAndWritesSameResource));
}

TEST(RenderGraphValidation, InvalidResourceIdDiagnostic)
{
    RG::RenderGraph graph;
    graph.AddPass("invalid-read",
                  {Read(RG::RGResourceId::kInvalid)},
                  {},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::InvalidResourceId));
}

TEST(RenderGraphValidation, InvalidOutputResourceIdDiagnostic)
{
    RG::RenderGraph graph;
    graph.AddPass("invalid-output",
                  {},
                  {Write(RG::RGResourceId::kInvalid)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());

    const auto& snapshot = graph.GetLastCompileSnapshot();
    ASSERT_EQ(snapshot.diagnostics.size(), 1u);
    EXPECT_EQ(snapshot.diagnostics[0].code,
              RG::RGDiagnosticCode::InvalidResourceId);
    EXPECT_EQ(snapshot.diagnostics[0].passIndex, 0u);
    EXPECT_EQ(snapshot.diagnostics[0].passName, "invalid-output");
    EXPECT_EQ(snapshot.diagnostics[0].resource,
              RG::RGResourceId::kInvalid);
}

TEST(RenderGraphValidation, ReadBeforeWriteDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("read",
                  {Read(color)},
                  {},
                  [](RHI::IRHI&) {});
    graph.AddPass("write",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::ReadBeforeWrite));
}

TEST(RenderGraphValidation, DuplicateWriterDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write-a",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});
    graph.AddPass("write-b",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::DuplicateWriter));
}

TEST(RenderGraphValidation, MissingProducerDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("read",
                  {Read(color)},
                  {},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::MissingOutputProducer));
}

TEST(RenderGraphValidation, EmptyPassNameDiagnostic)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("",
                  {},
                  {Write(color)},
                  [](RHI::IRHI&) {});

    EXPECT_FALSE(graph.Compile());
    EXPECT_TRUE(HasDiagnostic(graph.GetLastCompileSnapshot(),
                              RG::RGDiagnosticCode::EmptyPassName));
}

TEST(RenderGraphValidation, MultipleDiagnosticsKeepStableOrder)
{
    RG::RenderGraph graph;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("",
                  {},
                  {Write(RG::RGResourceId::kInvalid)},
                  [](RHI::IRHI&) {});
    graph.AddPass("missing-producer",
                  {Read(color)},
                  {},
                  [](RHI::IRHI&) {});
    graph.AddPass("invalid-read",
                  {Read(RG::RGResourceId::kInvalid)},
                  {},
                  [](RHI::IRHI&) {});

    ASSERT_FALSE(graph.Compile());
    const auto firstDump = graph.DumpLastCompile();

    ASSERT_FALSE(graph.Compile());
    const auto secondDump = graph.DumpLastCompile();
    const auto& diagnostics = graph.GetLastCompileSnapshot().diagnostics;

    ASSERT_EQ(diagnostics.size(), 4u);
    EXPECT_EQ(diagnostics[0].code, RG::RGDiagnosticCode::EmptyPassName);
    EXPECT_EQ(diagnostics[0].passIndex, 0u);
    EXPECT_EQ(diagnostics[1].code, RG::RGDiagnosticCode::InvalidResourceId);
    EXPECT_EQ(diagnostics[1].passIndex, 0u);
    EXPECT_EQ(diagnostics[2].code,
              RG::RGDiagnosticCode::MissingOutputProducer);
    EXPECT_EQ(diagnostics[2].passIndex, 1u);
    EXPECT_EQ(diagnostics[3].code, RG::RGDiagnosticCode::InvalidResourceId);
    EXPECT_EQ(diagnostics[3].passIndex, 2u);
    EXPECT_EQ(firstDump, secondDump);
    ExpectTextOrder(firstDump, "code=1 pass=0", "code=2 pass=0");
    ExpectTextOrder(firstDump, "code=2 pass=0", "code=5 pass=1");
    ExpectTextOrder(firstDump, "code=5 pass=1", "code=2 pass=2");
}

TEST(RenderGraphValidation, SuccessfulGraphCompileAndExecuteOrderUnchanged)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    std::vector<int> executionOrder;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write",
                  {},
                  {Write(color)},
                  [&executionOrder](RHI::IRHI&)
                  {
                      executionOrder.push_back(1);
                  });
    graph.AddPass("read",
                  {Read(color)},
                  {},
                  [&executionOrder](RHI::IRHI&)
                  {
                      executionOrder.push_back(2);
                  });

    ASSERT_TRUE(graph.Compile());
    graph.Execute(rhi);

    EXPECT_EQ(executionOrder, (std::vector<int>{1, 2}));
    EXPECT_TRUE(graph.GetLastCompileSnapshot().valid);
    EXPECT_TRUE(graph.GetLastCompileSnapshot().diagnostics.empty());

    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_TRUE(execution.attempted);
    EXPECT_TRUE(execution.executed);
    EXPECT_TRUE(execution.validCompile);
    EXPECT_EQ(execution.skipReason, RG::RGExecutionSkipReason::None);
    EXPECT_EQ(execution.executedPassCount, 2u);
    EXPECT_EQ(execution.passOrder, (std::vector<std::string>{"write", "read"}));
}

TEST(RenderGraphValidation, AddPassAfterCompileInvalidatesExecution)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    int executeCount = 0;

    graph.AddPass("first",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_TRUE(graph.Compile());
    graph.AddPass("second",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    EXPECT_TRUE(graph.IsCompiledStateDirty());
    EXPECT_FALSE(graph.HasCompiledState());
    EXPECT_FALSE(graph.IsCompiledStateValid());

    graph.Execute(rhi);

    EXPECT_EQ(executeCount, 0);
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_TRUE(execution.attempted);
    EXPECT_FALSE(execution.executed);
    EXPECT_FALSE(execution.validCompile);
    EXPECT_EQ(
        execution.skipReason,
        RG::RGExecutionSkipReason::CompiledStateInvalidated);
    EXPECT_EQ(execution.executedPassCount, 0u);
    EXPECT_TRUE(execution.passOrder.empty());
}

TEST(RenderGraphValidation, AddResourceAfterCompileInvalidatesExecution)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    int executeCount = 0;

    graph.AddPass("utility",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_TRUE(graph.Compile());
    const auto lateTexture = graph.AddTexture(TextureDesc("late"));
    EXPECT_NE(lateTexture, RG::RGResourceId::kInvalid);
    graph.Execute(rhi);

    EXPECT_EQ(executeCount, 0);
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_EQ(
        execution.skipReason,
        RG::RGExecutionSkipReason::CompiledStateInvalidated);
}

TEST(RenderGraphValidation, ClearAfterCompileReturnsToGraphNotCompiled)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    int executeCount = 0;

    graph.AddPass("utility",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_TRUE(graph.Compile());
    graph.Clear();
    graph.Execute(rhi);

    EXPECT_EQ(executeCount, 0);
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_EQ(
        execution.skipReason,
        RG::RGExecutionSkipReason::GraphNotCompiled);
}

TEST(RenderGraphValidation, RecompileAfterMutationRestoresExecution)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    std::vector<int> executionOrder;

    graph.AddPass("first",
                  [&executionOrder](RHI::IRHI&)
                  {
                      executionOrder.push_back(1);
                  });

    ASSERT_TRUE(graph.Compile());
    graph.AddPass("second",
                  [&executionOrder](RHI::IRHI&)
                  {
                      executionOrder.push_back(2);
                  });
    ASSERT_TRUE(graph.Compile());
    graph.Execute(rhi);

    EXPECT_EQ(executionOrder, (std::vector<int>{1, 2}));
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_TRUE(execution.executed);
    EXPECT_EQ(execution.skipReason, RG::RGExecutionSkipReason::None);
    EXPECT_EQ(execution.executedPassCount, 2u);
    EXPECT_EQ(execution.passOrder,
              (std::vector<std::string>{"first", "second"}));
}

TEST(RenderGraphValidation, InvalidCompileExecuteIsNoOp)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    int executeCount = 0;

    graph.AddPass("invalid",
                  {Read(RG::RGResourceId::kInvalid)},
                  {},
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_FALSE(graph.Compile());
    graph.Execute(rhi);

    EXPECT_EQ(executeCount, 0);
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_TRUE(execution.attempted);
    EXPECT_FALSE(execution.executed);
    EXPECT_FALSE(execution.validCompile);
    EXPECT_EQ(execution.skipReason, RG::RGExecutionSkipReason::InvalidCompile);
    EXPECT_EQ(execution.executedPassCount, 0u);
    EXPECT_TRUE(execution.passOrder.empty());
}

TEST(RenderGraphValidation, ExecuteBeforeCompileReportsGraphNotCompiled)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    int executeCount = 0;

    graph.AddPass("utility",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    graph.Execute(rhi);

    EXPECT_EQ(executeCount, 0);
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_TRUE(execution.attempted);
    EXPECT_FALSE(execution.executed);
    EXPECT_FALSE(execution.validCompile);
    EXPECT_EQ(
        execution.skipReason,
        RG::RGExecutionSkipReason::GraphNotCompiled);
    EXPECT_EQ(execution.executedPassCount, 0u);
}

TEST(RenderGraphValidation, DependencyFreePassOverloadExecutes)
{
    RG::RenderGraph graph;
    FakeRHI rhi;
    int executeCount = 0;

    graph.AddPass("utility",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_TRUE(graph.Compile());
    graph.Execute(rhi);

    EXPECT_EQ(executeCount, 1);
    const auto& execution = graph.GetLastExecutionSnapshot();
    EXPECT_TRUE(execution.executed);
    EXPECT_EQ(execution.executedPassCount, 1u);
    EXPECT_EQ(execution.passOrder, (std::vector<std::string>{"utility"}));
}

TEST(RenderGraphValidation, FrameExecutorWrapsGraphExecutionInFrameLifecycle)
{
    RG::RenderGraph graph;
    RG::RenderGraphFrameExecutor executor;
    RecordingRHI rhi;
    const auto color = graph.AddTexture(TextureDesc());

    graph.AddPass("write",
                  {},
                  {Write(color)},
                  [&rhi](RHI::IRHI&)
                  {
                      rhi.events.push_back("pass");
                  });

    ASSERT_TRUE(graph.Compile());
    const auto result = executor.Execute(graph, rhi);

    EXPECT_TRUE(result.attempted);
    EXPECT_TRUE(result.submitted);
    EXPECT_EQ(result.skipReason, RG::RGFrameExecutionSkipReason::None);
    EXPECT_EQ(result.executedPassCount, 1u);
    EXPECT_EQ(rhi.beginFrameCalls, 1u);
    EXPECT_EQ(rhi.endFrameCalls, 1u);
    EXPECT_EQ(rhi.events,
              (std::vector<std::string>{"begin", "pass", "end"}));
    EXPECT_TRUE(graph.GetLastExecutionSnapshot().executed);
}

TEST(RenderGraphValidation, FrameExecutorSkipsInvalidCompileWithoutSubmitting)
{
    RG::RenderGraph graph;
    RG::RenderGraphFrameExecutor executor;
    RecordingRHI rhi;
    int executeCount = 0;

    graph.AddPass("invalid",
                  {Read(RG::RGResourceId::kInvalid)},
                  {},
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_FALSE(graph.Compile());
    const auto result = executor.Execute(graph, rhi);

    EXPECT_TRUE(result.attempted);
    EXPECT_FALSE(result.submitted);
    EXPECT_EQ(result.skipReason, RG::RGFrameExecutionSkipReason::InvalidCompile);
    EXPECT_EQ(result.executedPassCount, 0u);
    EXPECT_EQ(executeCount, 0);
    EXPECT_EQ(rhi.beginFrameCalls, 0u);
    EXPECT_EQ(rhi.endFrameCalls, 0u);
    EXPECT_TRUE(rhi.events.empty());
}

TEST(RenderGraphValidation, FrameExecutorSkipsDirtyGraphWithoutSubmitting)
{
    RG::RenderGraph graph;
    RG::RenderGraphFrameExecutor executor;
    RecordingRHI rhi;
    int executeCount = 0;

    graph.AddPass("first",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_TRUE(graph.Compile());
    graph.AddPass("second",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    const auto result = executor.Execute(graph, rhi);

    EXPECT_TRUE(result.attempted);
    EXPECT_FALSE(result.submitted);
    EXPECT_EQ(
        result.skipReason,
        RG::RGFrameExecutionSkipReason::CompiledStateInvalidated);
    EXPECT_EQ(executeCount, 0);
    EXPECT_EQ(rhi.beginFrameCalls, 0u);
    EXPECT_EQ(rhi.endFrameCalls, 0u);
    EXPECT_TRUE(rhi.events.empty());
}

TEST(RenderGraphValidation, FrameExecutorSkipsInvalidFrameWithoutSubmitting)
{
    RG::RenderGraph graph;
    RG::RenderGraphFrameExecutor executor;
    RecordingRHI rhi;
    int executeCount = 0;

    graph.AddPass("utility",
                  [&executeCount](RHI::IRHI&)
                  {
                      ++executeCount;
                  });

    ASSERT_TRUE(graph.Compile());
    RG::RGFrameExecutionDesc desc{};
    desc.frameValid = false;
    const auto result = executor.Execute(graph, rhi, desc);

    EXPECT_TRUE(result.attempted);
    EXPECT_FALSE(result.submitted);
    EXPECT_EQ(result.skipReason, RG::RGFrameExecutionSkipReason::InvalidFrame);
    EXPECT_EQ(executeCount, 0);
    EXPECT_EQ(rhi.beginFrameCalls, 0u);
    EXPECT_EQ(rhi.endFrameCalls, 0u);
    EXPECT_TRUE(rhi.events.empty());
}
