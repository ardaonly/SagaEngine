#pragma once
#include <string>
namespace SagaEditor::Collaboration {
class ThreeWayMerge {
public:
    static std::string Merge(const std::string& base,
                              const std::string& local,
                              const std::string& remote);
};
} // namespace SagaEditor::Collaboration
