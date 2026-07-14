/// @file BlockVisualStyle.cpp
/// @brief Implementation of the visual scripting block style descriptor.

#include "SagaEditor/Persona/BlockVisualStyle.h"

namespace SagaEditor
{

// ─── Block Shape ──────────────────────────────────────────────────────────────

const char* BlockShapeId(BlockShape shape) noexcept
{
    switch (shape)
    {
        case BlockShape::Stacked: return "saga.block.stacked";
        case BlockShape::PinNode: return "saga.block.pin";
        case BlockShape::Hybrid:  return "saga.block.hybrid";
    }
    return "saga.block.unknown";
}

BlockShape BlockShapeFromId(const std::string& id) noexcept
{
    if (id == "saga.block.stacked") return BlockShape::Stacked;
    if (id == "saga.block.pin")     return BlockShape::PinNode;
    if (id == "saga.block.hybrid")  return BlockShape::Hybrid;
    return BlockShape::PinNode;
}

// ─── Connector Style ──────────────────────────────────────────────────────────

const char* ConnectorStyleId(ConnectorStyle style) noexcept
{
    switch (style)
    {
        case ConnectorStyle::Notched:    return "saga.connector.notched";
        case ConnectorStyle::Bezier:     return "saga.connector.bezier";
        case ConnectorStyle::Straight:   return "saga.connector.straight";
        case ConnectorStyle::Orthogonal: return "saga.connector.orthogonal";
    }
    return "saga.connector.unknown";
}

ConnectorStyle ConnectorStyleFromId(const std::string& id) noexcept
{
    if (id == "saga.connector.notched")    return ConnectorStyle::Notched;
    if (id == "saga.connector.bezier")     return ConnectorStyle::Bezier;
    if (id == "saga.connector.straight")   return ConnectorStyle::Straight;
    if (id == "saga.connector.orthogonal") return ConnectorStyle::Orthogonal;
    return ConnectorStyle::Bezier;
}

// ─── Slot Shape ───────────────────────────────────────────────────────────────

const char* SlotShapeId(SlotShape shape) noexcept
{
    switch (shape)
    {
        case SlotShape::Square:  return "saga.slot.square";
        case SlotShape::Pill:    return "saga.slot.pill";
        case SlotShape::Diamond: return "saga.slot.diamond";
        case SlotShape::Hexagon: return "saga.slot.hexagon";
    }
    return "saga.slot.unknown";
}

SlotShape SlotShapeFromId(const std::string& id) noexcept
{
    if (id == "saga.slot.square")  return SlotShape::Square;
    if (id == "saga.slot.pill")    return SlotShape::Pill;
    if (id == "saga.slot.diamond") return SlotShape::Diamond;
    if (id == "saga.slot.hexagon") return SlotShape::Hexagon;
    return SlotShape::Square;
}

// ─── Equality ─────────────────────────────────────────────────────────────────

bool BlockVisualStyle::operator==(const BlockVisualStyle& o) const noexcept
{
    return displayName       == o.displayName
        && shape             == o.shape
        && connectorStyle    == o.connectorStyle
        && defaultSlotShape  == o.defaultSlotShape
        && cornerRadius      == o.cornerRadius
        && headerHeight      == o.headerHeight
        && slotPadding       == o.slotPadding
        && minNodeWidth      == o.minNodeWidth
        && saturationScale   == o.saturationScale
        && shadowAlpha       == o.shadowAlpha
        && labelScale        == o.labelScale
        && showPinLabels     == o.showPinLabels
        && showPinTypes      == o.showPinTypes
        && showCategoryIcons == o.showCategoryIcons
        && connectorWidth    == o.connectorWidth
        && connectorTension  == o.connectorTension
        && paletteSwatchSize == o.paletteSwatchSize;
}

// ─── Factories ────────────────────────────────────────────────────────────────

BlockVisualStyle MakeStackedBlocksStyle()
{
    BlockVisualStyle s;
    s.displayName       = "Stacked Blocks";
    s.shape             = BlockShape::Stacked;
    s.connectorStyle    = ConnectorStyle::Notched;
    s.defaultSlotShape  = SlotShape::Pill;

    s.cornerRadius      = 10.0f;
    s.headerHeight      = 32.0f;
    s.slotPadding       = 8.0f;
    s.minNodeWidth      = 180.0f;

    s.saturationScale   = 1.25f; // friendly, vivid category colours.
    s.shadowAlpha       = 0.20f;

    s.labelScale        = 1.20f;
    s.showPinLabels     = false; // pins are hidden in stacked mode.
    s.showPinTypes      = false;
    s.showCategoryIcons = true;

    s.connectorWidth    = 4.0f;  // chunky to match block weight.
    s.connectorTension  = 0.0f;  // notched mode does not use tension.

    s.paletteSwatchSize = 36.0f;
    return s;
}

BlockVisualStyle MakePinNodeStyle()
{
    BlockVisualStyle s;
    s.displayName       = "Pin Node";
    s.shape             = BlockShape::PinNode;
    s.connectorStyle    = ConnectorStyle::Bezier;
    s.defaultSlotShape  = SlotShape::Square;

    s.cornerRadius      = 6.0f;
    s.headerHeight      = 22.0f;
    s.slotPadding       = 4.0f;
    s.minNodeWidth      = 140.0f;

    s.saturationScale   = 0.85f; // muted accents per pro-tools convention.
    s.shadowAlpha       = 0.35f;

    s.labelScale        = 1.0f;
    s.showPinLabels     = true;
    s.showPinTypes      = true;
    s.showCategoryIcons = false;

    s.connectorWidth    = 2.0f;
    s.connectorTension  = 0.5f;

    s.paletteSwatchSize = 24.0f;
    return s;
}

BlockVisualStyle MakeHybridStyle()
{
    BlockVisualStyle s = MakePinNodeStyle();
    s.displayName       = "Hybrid";
    s.shape             = BlockShape::Hybrid;
    s.connectorStyle    = ConnectorStyle::Bezier;
    s.defaultSlotShape  = SlotShape::Pill;
    s.saturationScale   = 1.0f;
    s.cornerRadius      = 8.0f;
    s.minNodeWidth      = 160.0f;
    s.showPinLabels     = true;
    s.showPinTypes      = false;
    s.showCategoryIcons = true;
    return s;
}

BlockVisualStyle MakeCompactPinStyle()
{
    BlockVisualStyle s = MakePinNodeStyle();
    s.displayName       = "Compact Pin";
    s.connectorStyle    = ConnectorStyle::Straight;
    s.cornerRadius      = 4.0f;
    s.headerHeight      = 18.0f;
    s.slotPadding       = 2.0f;
    s.minNodeWidth      = 100.0f;
    s.labelScale        = 0.9f;
    s.showPinLabels     = false; // hidden by default; show on hover.
    s.showPinTypes      = true;
    s.connectorWidth    = 1.5f;
    s.connectorTension  = 0.0f;
    s.paletteSwatchSize = 18.0f;
    return s;
}

} // namespace SagaEditor
