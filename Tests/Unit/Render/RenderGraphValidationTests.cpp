#include <gtest/gtest.h>

#include "SagaEngine/Render/RenderGraph/RenderGraph.h"
#include "SagaEngine/RHI/IRHI.h"

#include <algorithm>
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
