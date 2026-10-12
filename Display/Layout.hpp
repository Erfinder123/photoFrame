#ifndef LAYOUT_HPP
#define LAYOUT_HPP


#include <cstdint>


using ContainerId = uint16_t;

constexpr ContainerId rootContainerId = 0xFFFFU;

enum class ContainerType : uint8_t
{
    IMAGE,
    TEXT,
    ICON,
    GROUP
};

enum class HorizontalAnchor : uint8_t
{
    NONE,
    LEFT,
    CENTER,
    RIGHT,
    BEFORE,
    AFTER
};

enum class VerticalAnchor : uint8_t
{
    NONE,
    TOP,
    CENTER,
    BOTTOM,
    ABOVE,
    BELOW
};

enum class DisplacementDirection : uint8_t
{
    HORIZONTAL,
    VERTICAL
};

struct Position
{
    int32_t x = 0;
    int32_t y = 0;
};

struct Size
{
    uint32_t width = 0U;
    uint32_t height = 0U;
};

struct Rect
{
    Position position;
    Size size;
};

struct Anchor
{
    HorizontalAnchor horizontal = HorizontalAnchor::NONE;
    VerticalAnchor vertical = VerticalAnchor::NONE;
};

struct AnchorOffset
{
    int32_t horizontal = 0;
    int32_t vertical = 0;
};

struct Container
{
    ContainerId id = rootContainerId;
    ContainerId parentId = rootContainerId;
    ContainerId anchorTargetId = rootContainerId;
    const char* name = nullptr;
    ContainerType type = ContainerType::GROUP;
    Size size;
    Anchor anchor;
    AnchorOffset offset;
    uint8_t priority = 0U;
    uint8_t zIndex = 0U;
    uint16_t order = 0U;
    DisplacementDirection displacement = DisplacementDirection::HORIZONTAL;
    bool visible = true;
    Rect calculatedBounds;
    Rect visibleBounds;
};

class LayoutCalculator
{
    public:
        static constexpr uint32_t maximumContainerCount = 32U;

        [[nodiscard]] bool calculate(
            Container* containers,
            uint32_t containerCount,
            const Rect& screenBounds) const;

        [[nodiscard]] static bool intersects(
            const Rect& left,
            const Rect& right);
        [[nodiscard]] static Rect intersection(
            const Rect& left,
            const Rect& right);
        [[nodiscard]] static Rect unite(
            const Rect& left,
            const Rect& right);

    private:
        [[nodiscard]] static const Container* findCalculatedContainer(
            const Container* containers,
            uint32_t containerCount,
            ContainerId id,
            const bool* calculated);
        static void place(
            Container& container,
            const Rect& referenceBounds);
        static void displaceFrom(
            Container& container,
            const Container& obstacle);
};


#endif // LAYOUT_HPP
