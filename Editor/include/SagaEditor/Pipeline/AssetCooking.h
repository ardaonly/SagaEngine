#pragma once
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor {
struct CookJob   { std::string assetPath; std::string targetPlatform; };
struct CookResult{ bool success; std::string outputPath; std::string error; };
class AssetCooking {
public:
    AssetCooking();
    ~AssetCooking();
    CookResult Cook(const CookJob& job);
    void CookAsync(const CookJob& job, std::function<void(CookResult)> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
