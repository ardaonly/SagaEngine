#include <gtest/gtest.h>

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
    EXPECT_NE(firstDump.find("RenderGraphDump v0"), std::string::npos);
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
    EXPECT_NE(firstDump.find("RenderGraphDump v0"), std::string::npos);
    EXPECT_NE(firstDump.find("valid: true"), std::string::npos);
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

    EXPECT_NE(dump.find("1 buffer \"constants\""), std::string::npos);
    EXPECT_NE(dump.find("2 texture \"color\""), std::string::npos);
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
}
