#include "SagaEngine/Simulation/Deterministic.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::Simulation
{

// DeterministicRandom

DeterministicRandom::DeterministicRandom(uint64_t seed)
    : m_Seed(seed)
    , m_Rng(seed)
{
}

void DeterministicRandom::SetSeed(uint64_t seed)
{
    m_Seed = seed;
    m_Rng.seed(seed);
}

int DeterministicRandom::NextInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_Rng);
}

float DeterministicRandom::NextFloat(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_Rng);
}

double DeterministicRandom::NextDouble(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(m_Rng);
}

uint32_t DeterministicRandom::NextUInt32()
{
    std::uniform_int_distribution<uint32_t> dist;
    return dist(m_Rng);
}

uint64_t DeterministicRandom::NextUInt64()
{
    std::uniform_int_distribution<uint64_t> dist;
    return dist(m_Rng);
}

// DeterministicManager

DeterministicManager& DeterministicManager::Instance()
{
    static DeterministicManager instance;
    return instance;
}

DeterministicManager::DeterministicManager()
    : m_Random(m_GlobalSeed)
{
    LOG_INFO("DeterministicManager", "DeterministicManager initialized (seed: %llu)", m_GlobalSeed);
}

void DeterministicManager::SetGlobalSeed(uint64_t seed)
{
    m_GlobalSeed = seed;
    m_Random.SetSeed(seed);
    LOG_INFO("DeterministicManager", "Global seed set: %llu", seed);
}

DeterministicRandom& DeterministicManager::GetRandom()
{
    return m_Random;
}

} // namespace SagaEngine::Simulation