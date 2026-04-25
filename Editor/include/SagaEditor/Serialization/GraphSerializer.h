#pragma once
#include <string>
namespace SagaEditor {
class GraphDocument;
class GraphSerializer {
public:
    static bool SaveToFile(const GraphDocument& doc, const std::string& path);
    static bool LoadFromFile(GraphDocument& doc, const std::string& path);
    static std::string ToJson(const GraphDocument& doc);
    static bool FromJson(GraphDocument& doc, const std::string& json);
};
} // namespace SagaEditor
