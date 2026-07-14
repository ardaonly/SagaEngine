#pragma once
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor {
class WorldStreamingEditor {
public:
    WorldStreamingEditor();
    ~WorldStreamingEditor();
    void SetStreamingRadius(float radius);
    float GetStreamingRadius() const noexcept;
    void RefreshStreamingVolumes();
    void SetOnStreamingChanged(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
