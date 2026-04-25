#pragma once
#include <memory>
#include <string>
#include <vector>
namespace SagaEditor::VisualScripting {
struct NodeId  { uint64_t value; };
struct PinId   { uint64_t value; };
struct LinkId  { uint64_t value; };
struct NodeData  { NodeId id; std::string type; float x=0,y=0; std::string label; };
struct LinkData  { LinkId id; PinId fromPin; PinId toPin; };
class GraphDocument {
public:
    GraphDocument();
    ~GraphDocument();
    NodeId AddNode(const std::string& type, float x=0, float y=0);
    void   RemoveNode(NodeId id);
    LinkId AddLink(PinId from, PinId to);
    void   RemoveLink(LinkId id);
    const std::vector<NodeData>& GetNodes() const noexcept;
    const std::vector<LinkData>& GetLinks() const noexcept;
    bool   IsModified() const noexcept;
    void   MarkClean();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::VisualScripting
