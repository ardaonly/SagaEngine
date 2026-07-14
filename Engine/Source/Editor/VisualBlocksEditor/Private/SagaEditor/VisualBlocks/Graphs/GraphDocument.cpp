/// @file GraphDocument.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Graphs/GraphDocument.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct GraphDocument::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

GraphDocument::GraphDocument()
    : m_impl(std::make_unique<Impl>())
{}

GraphDocument::~GraphDocument() = default;

NodeId GraphDocument::AddNode(const std::string& /*type*/, float /*x*/, float /*y*/)
{
    return {};
}

void GraphDocument::RemoveNode(NodeId /*id*/)
{
    /* stub */
}

LinkId GraphDocument::AddLink(PinId /*from*/, PinId /*to*/)
{
    return {};
}

void GraphDocument::RemoveLink(LinkId /*id*/)
{
    /* stub */
}

const std::vector<NodeData>& GraphDocument::GetNodes() const noexcept
{
    static std::vector<NodeData> s_default; return s_default;
}

const std::vector<LinkData>& GraphDocument::GetLinks() const noexcept
{
    static std::vector<LinkData> s_default; return s_default;
}

bool GraphDocument::IsModified() const noexcept
{
    return false;
}

void GraphDocument::MarkClean()
{
    /* stub */
}

} // namespace SagaEditor::VisualScripting
