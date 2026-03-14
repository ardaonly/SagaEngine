#pragma once
#include <cstdint>
#include <random>

namespace SagaEngine::Simulation
{

class DeterministicRandom
{
public:
    explicit DeterministicRandom(uint64_t seed = 0);
    
    void SetSeed(uint64_t seed);
    uint64_t GetSeed() const { return m_Seed; }
    
    int NextInt(int min, int max);
    float NextFloat(float min, float max);
    double NextDouble(double min, double max);
    
    uint32_t NextUInt32();
    uint64_t NextUInt64();
    
private:
    uint64_t m_Seed;
    std::mt19937_64 m_Rng;
};

class DeterministicManager
{
public:
    static DeterministicManager& Instance();
    
    void SetGlobalSeed(uint64_t seed);
    uint64_t GetGlobalSeed() const { return m_GlobalSeed; }
    
    DeterministicRandom& GetRandom();
    
    void SetTickCount(uint64_t count) { m_TickCount = count; }
    uint64_t GetTickCount() const { return m_TickCount; }
    
    bool IsDeterministic() const { return m_IsDeterministic; }
    void SetDeterministic(bool deterministic) { m_IsDeterministic = deterministic; }
    
private:
    DeterministicManager();
    
    uint64_t m_GlobalSeed{12345};
    uint64_t m_TickCount{0};
    bool m_IsDeterministic{true};
    DeterministicRandom m_Random;
};

} // namespace SagaEngine::Simulation