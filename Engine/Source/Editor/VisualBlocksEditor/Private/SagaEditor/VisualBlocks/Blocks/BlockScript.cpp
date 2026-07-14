/// @file BlockScript.cpp
/// @brief Owning container for instances + stacks of a single block program.

#include "SagaEditor/VisualScripting/Blocks/BlockScript.h"

#include "SagaEditor/VisualScripting/Blocks/BlockLibrary.h"

#include <algorithm>
#include <utility>

namespace SagaEditor::VisualScripting
{

// ─── Internal Storage ─────────────────────────────────────────────────────────

struct BlockScript::Impl
{
    std::unordered_map<std::uint64_t, BlockInstance> instances;
    std::unordered_map<std::uint64_t, BlockStack>    stacks;
    std::vector<BlockStackId>                        rootStacks;
    std::uint64_t                                    nextInstanceId = 1;
    std::uint64_t                                    nextStackId    = 1;
};

// ─── Construction ─────────────────────────────────────────────────────────────

BlockScript::BlockScript()
    : m_impl(std::make_unique<Impl>())
{}

BlockScript::~BlockScript() = default;

BlockScript::BlockScript(BlockScript&&) noexcept            = default;
BlockScript& BlockScript::operator=(BlockScript&&) noexcept = default;

// ─── Mutation ─────────────────────────────────────────────────────────────────

BlockInstanceId BlockScript::CreateInstance(std::string opcode)
{
    BlockInstance inst;
    inst.id.value = m_impl->nextInstanceId++;
    inst.opcode   = std::move(opcode);
    const auto id = inst.id;
    m_impl->instances.emplace(id.value, std::move(inst));
    return id;
}

BlockStackId BlockScript::CreateStack(BlockInstanceId head)
{
    BlockStack stack;
    stack.id.value = m_impl->nextStackId++;
    if (head.IsValid())
    {
        stack.instances.push_back(head);
    }
    const auto id = stack.id;
    m_impl->stacks.emplace(id.value, std::move(stack));
    return id;
}

void BlockScript::AddRootStack(BlockStackId stack)
{
    if (!stack.IsValid()) return;
    if (std::find(m_impl->rootStacks.begin(), m_impl->rootStacks.end(),
                  stack) == m_impl->rootStacks.end())
    {
        m_impl->rootStacks.push_back(stack);
    }
}

void BlockScript::RemoveRootStack(BlockStackId stack) noexcept
{
    auto& roots = m_impl->rootStacks;
    roots.erase(std::remove(roots.begin(), roots.end(), stack), roots.end());
}

bool BlockScript::AppendToStack(BlockStackId stackId, BlockInstanceId instance)
{
    auto* stack = FindStack(stackId);
    if (stack == nullptr || !instance.IsValid()) return false;
    if (FindInstance(instance) == nullptr)        return false;
    stack->instances.push_back(instance);
    return true;
}

bool BlockScript::SetSlotFill(BlockInstanceId instanceId,
                              const std::string& slotId,
                              BlockSlotFill fill)
{
    auto* inst = FindInstance(instanceId);
    if (inst == nullptr) return false;
    inst->slotFills[slotId] = std::move(fill);
    return true;
}

bool BlockScript::AttachMouth(BlockInstanceId parentId,
                              std::uint8_t    mouthIndex,
                              BlockStackId    childStack)
{
    auto* parent = FindInstance(parentId);
    if (parent == nullptr) return false;
    if (parent->mouthStacks.size() <= mouthIndex)
    {
        parent->mouthStacks.resize(static_cast<std::size_t>(mouthIndex) + 1);
    }
    parent->mouthStacks[mouthIndex] = childStack;
    return true;
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

const BlockInstance* BlockScript::FindInstance(BlockInstanceId id) const noexcept
{
    if (!id.IsValid()) return nullptr;
    const auto it = m_impl->instances.find(id.value);
    return it == m_impl->instances.end() ? nullptr : &it->second;
}

BlockInstance* BlockScript::FindInstance(BlockInstanceId id) noexcept
{
    if (!id.IsValid()) return nullptr;
    const auto it = m_impl->instances.find(id.value);
    return it == m_impl->instances.end() ? nullptr : &it->second;
}

const BlockStack* BlockScript::FindStack(BlockStackId id) const noexcept
{
    if (!id.IsValid()) return nullptr;
    const auto it = m_impl->stacks.find(id.value);
    return it == m_impl->stacks.end() ? nullptr : &it->second;
}

BlockStack* BlockScript::FindStack(BlockStackId id) noexcept
{
    if (!id.IsValid()) return nullptr;
    const auto it = m_impl->stacks.find(id.value);
    return it == m_impl->stacks.end() ? nullptr : &it->second;
}

std::size_t BlockScript::InstanceCount() const noexcept
{
    return m_impl->instances.size();
}

std::size_t BlockScript::StackCount() const noexcept
{
    return m_impl->stacks.size();
}

std::vector<BlockStackId> BlockScript::GetRootStacks() const
{
    return m_impl->rootStacks;
}

// ─── Validation ───────────────────────────────────────────────────────────────

std::vector<BlockScript::ValidationIssue>
BlockScript::Validate(const BlockLibrary& library) const
{
    std::vector<ValidationIssue> issues;

    // Walk every instance, then every root stack, surfacing
    // structural issues. The walk is independent so we always emit
    // the most informative diagnostic regardless of which problem
    // came first.

    for (const auto& [_, inst] : m_impl->instances)
    {
        const BlockDefinition* def = library.FindDefinition(inst.opcode);
        if (def == nullptr)
        {
            ValidationIssue is;
            is.severity = ValidationIssue::Severity::Error;
            is.message  = "unknown opcode: " + inst.opcode;
            is.instance = inst.id;
            issues.push_back(std::move(is));
            continue;
        }

        const std::uint8_t expected = BlockKindMouthCount(def->kind);
        if (inst.mouthStacks.size() > expected)
        {
            ValidationIssue is;
            is.severity = ValidationIssue::Severity::Warning;
            is.message  = "too many mouth child stacks for opcode " + inst.opcode;
            is.instance = inst.id;
            issues.push_back(std::move(is));
        }

        // Slot expressions must point to Reporter / Boolean instances.
        for (const auto& [slotId, fill] : inst.slotFills)
        {
            if (fill.kind != BlockSlotFill::Kind::Expression) continue;
            const BlockInstance* exprInst = nullptr;
            const auto it = m_impl->instances.find(fill.expressionId);
            if (it != m_impl->instances.end()) exprInst = &it->second;
            if (exprInst == nullptr)
            {
                ValidationIssue is;
                is.severity = ValidationIssue::Severity::Error;
                is.message  = "slot " + slotId + " references missing expression";
                is.instance = inst.id;
                issues.push_back(std::move(is));
                continue;
            }
            const BlockDefinition* exprDef = library.FindDefinition(exprInst->opcode);
            if (exprDef != nullptr && !BlockKindIsExpression(exprDef->kind))
            {
                ValidationIssue is;
                is.severity = ValidationIssue::Severity::Error;
                is.message  = "slot " + slotId + " carries non-expression block";
                is.instance = inst.id;
                issues.push_back(std::move(is));
            }
        }
    }

    for (const auto& [_, stack] : m_impl->stacks)
    {
        if (stack.instances.empty()) continue;
        const auto headId = stack.instances.front();
        const auto* head = FindInstance(headId);
        if (head == nullptr) continue;
        const BlockDefinition* headDef = library.FindDefinition(head->opcode);
        if (headDef != nullptr && BlockKindIsExpression(headDef->kind))
        {
            ValidationIssue is;
            is.severity = ValidationIssue::Severity::Error;
            is.message  = "stack head must not be an expression block";
            is.stack    = stack.id;
            is.instance = headId;
            issues.push_back(std::move(is));
        }

        for (std::size_t i = 1; i < stack.instances.size(); ++i)
        {
            const auto* inst = FindInstance(stack.instances[i]);
            if (inst == nullptr) continue;
            const BlockDefinition* d = library.FindDefinition(inst->opcode);
            if (d == nullptr) continue;
            if (d->kind == BlockKind::Hat)
            {
                ValidationIssue is;
                is.severity = ValidationIssue::Severity::Error;
                is.message  = "Hat block must be at the top of a stack";
                is.stack    = stack.id;
                is.instance = stack.instances[i];
                issues.push_back(std::move(is));
            }
        }

        // Cap blocks may only be at the bottom.
        for (std::size_t i = 0; i + 1 < stack.instances.size(); ++i)
        {
            const auto* inst = FindInstance(stack.instances[i]);
            if (inst == nullptr) continue;
            const BlockDefinition* d = library.FindDefinition(inst->opcode);
            if (d != nullptr && d->kind == BlockKind::Cap)
            {
                ValidationIssue is;
                is.severity = ValidationIssue::Severity::Error;
                is.message  = "Cap block must end its stack";
                is.stack    = stack.id;
                is.instance = stack.instances[i];
                issues.push_back(std::move(is));
            }
        }
    }

    return issues;
}

} // namespace SagaEditor::VisualScripting
