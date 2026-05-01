/// @file BlockKind.cpp
/// @brief Implementation of the Scratch-style block shape vocabulary.

#include "SagaEditor/VisualScripting/Blocks/BlockKind.h"

namespace SagaEditor::VisualScripting
{

// ─── Identity ─────────────────────────────────────────────────────────────────

const char* BlockKindId(BlockKind kind) noexcept
{
    switch (kind)
    {
        case BlockKind::Hat:      return "saga.blockkind.hat";
        case BlockKind::Stack:    return "saga.blockkind.stack";
        case BlockKind::CBlock:   return "saga.blockkind.c";
        case BlockKind::EBlock:   return "saga.blockkind.e";
        case BlockKind::Cap:      return "saga.blockkind.cap";
        case BlockKind::Reporter: return "saga.blockkind.reporter";
        case BlockKind::Boolean:  return "saga.blockkind.boolean";
    }
    return "saga.blockkind.unknown";
}

BlockKind BlockKindFromId(const std::string& id) noexcept
{
    if (id == "saga.blockkind.hat")      return BlockKind::Hat;
    if (id == "saga.blockkind.stack")    return BlockKind::Stack;
    if (id == "saga.blockkind.c")        return BlockKind::CBlock;
    if (id == "saga.blockkind.e")        return BlockKind::EBlock;
    if (id == "saga.blockkind.cap")      return BlockKind::Cap;
    if (id == "saga.blockkind.reporter") return BlockKind::Reporter;
    if (id == "saga.blockkind.boolean")  return BlockKind::Boolean;
    return BlockKind::Stack;
}

} // namespace SagaEditor::VisualScripting
