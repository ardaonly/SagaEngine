/// @file BlockVisualStyle.h
/// @brief Visual scripting block / node rendering parameters per persona.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor
{

// ─── Block Shape ──────────────────────────────────────────────────────────────

/// Top-level visual mode the graph canvas renders nodes in.
/// `Stacked` matches the kid-friendly snap-together blocks layout and
/// hides typed pin connectors entirely. `PinNode` matches the
/// professional rounded-rectangle node with explicit typed input and
/// output pins, bezier connections, and an exposed expression
/// editor. `Hybrid` lets the user mix both inside the same graph
/// (used by the IndieBalanced persona).
enum class BlockShape : std::uint8_t
{
    Stacked,
    PinNode,
    Hybrid,
};

[[nodiscard]] const char* BlockShapeId(BlockShape shape) noexcept;
[[nodiscard]] BlockShape  BlockShapeFromId(const std::string& id) noexcept;

// ─── Connector Style ──────────────────────────────────────────────────────────

/// How connections between nodes / blocks are drawn. The set is
/// intentionally small — the canvas renderer special-cases each
/// option, so adding one here is a feature rather than a parameter.
enum class ConnectorStyle : std::uint8_t
{
    /// Snap-together top-bottom interlocking notches; matches Stacked.
    Notched,
    /// Curved bezier between pin centres; matches PinNode.
    Bezier,
    /// Straight line between pin centres; useful for compact layouts.
    Straight,
    /// Right-angle elbow with an axis-aligned mid segment.
    Orthogonal,
};

[[nodiscard]] const char*    ConnectorStyleId(ConnectorStyle style) noexcept;
[[nodiscard]] ConnectorStyle ConnectorStyleFromId(const std::string& id) noexcept;

// ─── Slot Shape ───────────────────────────────────────────────────────────────

/// Shape used to render the inline literal-input slots on a node.
/// Stacked blocks usually use `Pill` (rounded ends) for a friendly
/// feel; PinNode usually uses `Square`.
enum class SlotShape : std::uint8_t
{
    Square,
    Pill,
    Diamond,    ///< Reserved for boolean slots.
    Hexagon,    ///< Reserved for trigger / event slots.
};

[[nodiscard]] const char* SlotShapeId(SlotShape shape) noexcept;
[[nodiscard]] SlotShape   SlotShapeFromId(const std::string& id) noexcept;

// ─── Block Visual Style ───────────────────────────────────────────────────────

/// Numeric-and-enum description of how the graph canvas renders
/// blocks under the active persona. The struct stays POD so themes
/// and persona JSON files can value-compare it during diffs.
struct BlockVisualStyle
{
    std::string    displayName       = "PinNode Default";
    BlockShape     shape             = BlockShape::PinNode;
    ConnectorStyle connectorStyle    = ConnectorStyle::Bezier;
    SlotShape      defaultSlotShape  = SlotShape::Square;

    // Layout metrics.
    float          cornerRadius      = 6.0f;   ///< Block / node body corner radius (px).
    float          headerHeight      = 22.0f;  ///< Title strip height.
    float          slotPadding       = 4.0f;   ///< Padding around inline slots.
    float          minNodeWidth      = 120.0f; ///< Floor on node width to keep labels legible.

    // Colour treatment.
    float          saturationScale   = 1.0f;   ///< Multiplier on category-colour saturation.
    float          shadowAlpha       = 0.30f;  ///< Drop-shadow alpha in [0,1].

    // Label treatment.
    float          labelScale        = 1.0f;   ///< Multiplier on the density's font size.
    bool           showPinLabels     = true;   ///< Render typed pin labels next to handles.
    bool           showPinTypes      = true;   ///< Show pin type letters on hover.
    bool           showCategoryIcons = false;  ///< Show a category glyph in the header.

    // Connector metrics.
    float          connectorWidth    = 2.0f;   ///< Stroke width for connections.
    float          connectorTension  = 0.5f;   ///< Bezier tension in [0, 1].

    // Palette / bin layout.
    float          paletteSwatchSize = 28.0f;  ///< Drag-handle preview swatch in node palette.

    // ─── Equality ─────────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(const BlockVisualStyle& other) const noexcept;
    [[nodiscard]] bool operator!=(const BlockVisualStyle& other) const noexcept
    {
        return !(*this == other);
    }
};

// ─── Built-in Visual Styles ───────────────────────────────────────────────────

/// Chunky stacked blocks with notched connectors and high saturation.
/// Hides typed pin labels and types so beginners aren't asked to
/// reason about pin compatibility.
[[nodiscard]] BlockVisualStyle MakeStackedBlocksStyle();

/// Rounded rectangle node with bezier connections, typed pins
/// labelled and coloured. The default for ProDense and Technical.
[[nodiscard]] BlockVisualStyle MakePinNodeStyle();

/// Hybrid look — uses pin-node connectors on logic nodes but draws
/// snap-together stacked blocks for control-flow scaffolding. Used
/// by IndieBalanced.
[[nodiscard]] BlockVisualStyle MakeHybridStyle();

/// Compact pin-node variant tuned for the Dense / Ultra densities;
/// hides pin labels by default and uses straight connectors.
[[nodiscard]] BlockVisualStyle MakeCompactPinStyle();

} // namespace SagaEditor
