/// @file BlockLibrary.cpp
/// @brief Implementation of the block-library registry + built-ins.

#include "SagaEditor/VisualScripting/Blocks/BlockLibrary.h"

#include <algorithm>
#include <utility>

namespace SagaEditor::VisualScripting
{

namespace
{

// ─── Built-in Definition Helpers ──────────────────────────────────────────────

/// Construct a Stack-shape block with one numeric slot.
[[nodiscard]] BlockDefinition MakeStackOneNumber(std::string opcode,
                                                 std::string category,
                                                 std::string display,
                                                 std::string preLabel,
                                                 std::string postLabel,
                                                 std::string slotId,
                                                 double      defaultValue)
{
    BlockDefinition d;
    d.opcode       = std::move(opcode);
    d.categoryId   = std::move(category);
    d.displayName  = display;
    d.kind         = BlockKind::Stack;
    d.outputType   = BlockOutputType::None;

    if (!preLabel.empty())
    {
        d.labelFragments.push_back(BlockLabelFragment::MakeLiteral(preLabel));
    }
    d.labelFragments.push_back(BlockLabelFragment::MakeSlot(slotId));
    if (!postLabel.empty())
    {
        d.labelFragments.push_back(BlockLabelFragment::MakeLiteral(postLabel));
    }

    BlockSlotDefinition slot;
    slot.id            = std::move(slotId);
    slot.kind          = BlockSlotKind::Number;
    slot.defaultNumber = defaultValue;
    d.slots.push_back(std::move(slot));
    return d;
}

[[nodiscard]] BlockDefinition MakeHat(std::string opcode,
                                      std::string category,
                                      std::string display,
                                      std::string eventId)
{
    BlockDefinition d;
    d.opcode       = std::move(opcode);
    d.categoryId   = std::move(category);
    d.displayName  = display;
    d.kind         = BlockKind::Hat;
    d.outputType   = BlockOutputType::None;
    d.hatEventId   = std::move(eventId);
    d.labelFragments.push_back(BlockLabelFragment::MakeLiteral(display));
    return d;
}

[[nodiscard]] BlockDefinition MakeCBlock(std::string opcode,
                                         std::string category,
                                         std::string display,
                                         std::string slotId,
                                         BlockSlotKind slotKind)
{
    BlockDefinition d;
    d.opcode      = std::move(opcode);
    d.categoryId  = std::move(category);
    d.displayName = display;
    d.kind        = BlockKind::CBlock;
    d.outputType  = BlockOutputType::None;
    d.mouthLabels = { "do" };

    d.labelFragments.push_back(BlockLabelFragment::MakeLiteral(display));
    if (!slotId.empty())
    {
        d.labelFragments.push_back(BlockLabelFragment::MakeSlot(slotId));
        BlockSlotDefinition s;
        s.id = std::move(slotId);
        s.kind = slotKind;
        if (slotKind == BlockSlotKind::Number) s.defaultNumber = 10.0;
        d.slots.push_back(std::move(s));
    }
    return d;
}

[[nodiscard]] BlockDefinition MakeReporter(std::string opcode,
                                           std::string category,
                                           std::string display,
                                           BlockOutputType output)
{
    BlockDefinition d;
    d.opcode      = std::move(opcode);
    d.categoryId  = std::move(category);
    d.displayName = display;
    d.kind        = (output == BlockOutputType::Boolean)
                    ? BlockKind::Boolean : BlockKind::Reporter;
    d.outputType  = output;
    d.labelFragments.push_back(BlockLabelFragment::MakeLiteral(display));
    return d;
}

} // namespace

// ─── Population ───────────────────────────────────────────────────────────────

void BlockLibrary::RegisterBuiltinCategories()
{
    for (auto& cat : MakeBuiltinCategories())
    {
        RegisterCategory(std::move(cat));
    }
}

void BlockLibrary::RegisterBuiltinDefinitions()
{
    // Hats (Events).
    RegisterDefinition(MakeHat("saga.block.events.when_flag_clicked",
                               "saga.cat.events", "when flag clicked",
                               "saga.event.flag"));
    RegisterDefinition(MakeHat("saga.block.events.when_key_pressed",
                               "saga.cat.events", "when key pressed",
                               "saga.event.key"));

    // Motion stack blocks.
    RegisterDefinition(MakeStackOneNumber(
        "saga.block.motion.move_steps",
        "saga.cat.motion", "move N steps",
        "move ", " steps", "STEPS", 10.0));
    RegisterDefinition(MakeStackOneNumber(
        "saga.block.motion.turn_right",
        "saga.cat.motion", "turn right N degrees",
        "turn right ", " degrees", "DEGREES", 15.0));

    // Looks stack block: "say <text>".
    {
        BlockDefinition d;
        d.opcode      = "saga.block.looks.say";
        d.categoryId  = "saga.cat.looks";
        d.displayName = "say <text>";
        d.kind        = BlockKind::Stack;
        d.outputType  = BlockOutputType::None;
        d.labelFragments.push_back(BlockLabelFragment::MakeLiteral("say "));
        d.labelFragments.push_back(BlockLabelFragment::MakeSlot("MESSAGE"));

        BlockSlotDefinition s;
        s.id          = "MESSAGE";
        s.kind        = BlockSlotKind::Text;
        s.defaultText = "Hello!";
        d.slots.push_back(std::move(s));
        RegisterDefinition(std::move(d));
    }

    // Control flow.
    RegisterDefinition(MakeCBlock("saga.block.control.repeat",
                                  "saga.cat.control", "repeat",
                                  "TIMES", BlockSlotKind::Number));
    RegisterDefinition(MakeCBlock("saga.block.control.if",
                                  "saga.cat.control", "if",
                                  "CONDITION", BlockSlotKind::Boolean));

    // Cap.
    {
        BlockDefinition d;
        d.opcode      = "saga.block.control.stop_all";
        d.categoryId  = "saga.cat.control";
        d.displayName = "stop all";
        d.kind        = BlockKind::Cap;
        d.outputType  = BlockOutputType::None;
        d.labelFragments.push_back(BlockLabelFragment::MakeLiteral("stop all"));
        RegisterDefinition(std::move(d));
    }

    // Reporters.
    RegisterDefinition(MakeReporter("saga.block.operators.add",
                                    "saga.cat.operators", "+",
                                    BlockOutputType::Number));
    RegisterDefinition(MakeReporter("saga.block.sensing.touching_edge",
                                    "saga.cat.sensing", "touching edge?",
                                    BlockOutputType::Boolean));
    RegisterDefinition(MakeReporter("saga.block.variables.get",
                                    "saga.cat.variables", "<variable>",
                                    BlockOutputType::Any));
}

void BlockLibrary::RegisterAllBuiltins()
{
    RegisterBuiltinCategories();
    RegisterBuiltinDefinitions();
}

bool BlockLibrary::RegisterCategory(BlockCategory category)
{
    if (category.id.empty())
    {
        return false;
    }
    const std::string key = category.id;
    m_categories.insert_or_assign(key, std::move(category));
    return true;
}

bool BlockLibrary::RegisterDefinition(BlockDefinition definition)
{
    if (definition.opcode.empty())
    {
        return false;
    }
    if (!definition.categoryId.empty() &&
        m_categories.find(definition.categoryId) == m_categories.end())
    {
        return false;
    }
    const std::string key = definition.opcode;
    m_definitions.insert_or_assign(key, std::move(definition));
    return true;
}

void BlockLibrary::Clear() noexcept
{
    m_categories.clear();
    m_definitions.clear();
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

bool BlockLibrary::HasCategory(const std::string& id) const noexcept
{
    return m_categories.find(id) != m_categories.end();
}

bool BlockLibrary::HasDefinition(const std::string& opcode) const noexcept
{
    return m_definitions.find(opcode) != m_definitions.end();
}

const BlockCategory* BlockLibrary::FindCategory(const std::string& id) const noexcept
{
    const auto it = m_categories.find(id);
    return it == m_categories.end() ? nullptr : &it->second;
}

const BlockDefinition* BlockLibrary::FindDefinition(const std::string& opcode) const noexcept
{
    const auto it = m_definitions.find(opcode);
    return it == m_definitions.end() ? nullptr : &it->second;
}

std::vector<BlockCategory> BlockLibrary::GetAllCategories() const
{
    std::vector<BlockCategory> out;
    out.reserve(m_categories.size());
    for (const auto& [_, c] : m_categories)
    {
        out.push_back(c);
    }
    std::sort(out.begin(), out.end(),
              [](const BlockCategory& a, const BlockCategory& b) noexcept
              {
                  if (a.sortOrder != b.sortOrder) return a.sortOrder < b.sortOrder;
                  return a.id < b.id;
              });
    return out;
}

std::vector<BlockDefinition>
BlockLibrary::GetDefinitionsForCategory(const std::string& categoryId) const
{
    std::vector<BlockDefinition> out;
    for (const auto& [_, d] : m_definitions)
    {
        if (d.categoryId == categoryId)
        {
            out.push_back(d);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const BlockDefinition& a, const BlockDefinition& b) noexcept
              {
                  return a.displayName < b.displayName;
              });
    return out;
}

std::vector<BlockDefinition> BlockLibrary::GetAllDefinitions() const
{
    std::vector<BlockDefinition> out;
    out.reserve(m_definitions.size());
    for (const auto& [_, d] : m_definitions)
    {
        out.push_back(d);
    }
    return out;
}

std::size_t BlockLibrary::CategoryCount() const noexcept
{
    return m_categories.size();
}

std::size_t BlockLibrary::DefinitionCount() const noexcept
{
    return m_definitions.size();
}

} // namespace SagaEditor::VisualScripting
