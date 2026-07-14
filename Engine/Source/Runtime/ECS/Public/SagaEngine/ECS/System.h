#pragma once
#include "Entity.h"
#include "Component.h"
#include <memory>
#include <vector>
#include <string>

namespace SagaEngine::Simulation { class WorldState; }

namespace SagaEngine::ECS
{

class ISystem
{
public:
    virtual ~ISystem();
    virtual const char* GetName() const = 0;
    virtual void SetWorldState(Simulation::WorldState* world) { m_World = world; }
    virtual void OnSystemInit() = 0;
    virtual void OnSystemUpdate(float dt) = 0;
    virtual void OnSystemShutdown() = 0;
    virtual int GetPriority() const { return 0; }
    
protected:
    Simulation::WorldState* m_World{nullptr};
};

template<typename T>
class System : public ISystem
{
public:
    const char* GetName() const override
    {
        static const char* s_Name = typeid(T).name();
        return s_Name;
    }
};

} // namespace SagaEngine::ECS