#include "Layout.hpp"

#include <algorithm>


namespace
{
int32_t right(const Rect& rectangle)
{
    return rectangle.position.x + static_cast<int32_t>(rectangle.size.width);
}

int32_t bottom(const Rect& rectangle)
{
    return rectangle.position.y + static_cast<int32_t>(rectangle.size.height);
}
}

bool LayoutCalculator::calculate(
    Container* const containers,
    const uint32_t containerCount,
    const Rect& screenBounds) const
{
    if (containers == nullptr || containerCount > maximumContainerCount)
        return false;

    bool calculated[maximumContainerCount]{};
    uint32_t calculatedCount = 0U;
    while (calculatedCount < containerCount)
    {
        uint32_t selected = containerCount;
        const Container* selectedParent = nullptr;
        const Container* selectedAnchorTarget = nullptr;
        for (uint32_t index = 0U; index < containerCount; ++index)
        {
            if (calculated[index])
                continue;

            const Container* const parent = findCalculatedContainer(
                containers,
                containerCount,
                containers[index].parentId,
                calculated);
            if (containers[index].parentId != rootContainerId
                && parent == nullptr)
                continue;

            const Container* const anchorTarget = findCalculatedContainer(
                containers,
                containerCount,
                containers[index].anchorTargetId,
                calculated);
            if (containers[index].anchorTargetId != rootContainerId
                && anchorTarget == nullptr)
                continue;

            if (selected == containerCount
                || containers[index].order < containers[selected].order)
            {
                selected = index;
                selectedParent = parent;
                selectedAnchorTarget = anchorTarget;
            }
        }

        if (selected == containerCount)
            return false;

        Container& container = containers[selected];
        const Rect parentBounds = selectedParent == nullptr
                                ? screenBounds
                                : selectedParent->visibleBounds;
        const Rect anchorBounds = selectedAnchorTarget == nullptr
                                ? parentBounds
                                : selectedAnchorTarget->calculatedBounds;
        place(container, anchorBounds);

        for (uint32_t index = 0U; index < containerCount; ++index)
        {
            if (!calculated[index]
                || containers[index].parentId != container.parentId
                || containers[index].priority != container.priority)
                continue;

            if (intersects(container.calculatedBounds,
                           containers[index].calculatedBounds))
                displaceFrom(container, containers[index]);
        }

        container.visibleBounds = container.visible
                                ? intersection(container.calculatedBounds, parentBounds)
                                : Rect{};
        calculated[selected] = true;
        ++calculatedCount;
    }

    return true;
}

bool LayoutCalculator::intersects(const Rect& left, const Rect& rightRectangle)
{
    return left.size.width != 0U
        && left.size.height != 0U
        && rightRectangle.size.width != 0U
        && rightRectangle.size.height != 0U
        && left.position.x < right(rightRectangle)
        && right(left) > rightRectangle.position.x
        && left.position.y < bottom(rightRectangle)
        && bottom(left) > rightRectangle.position.y;
}

Rect LayoutCalculator::intersection(const Rect& left, const Rect& rightRectangle)
{
    const int32_t intersectionLeft = std::max(
        left.position.x,
        rightRectangle.position.x);
    const int32_t intersectionTop = std::max(
        left.position.y,
        rightRectangle.position.y);
    const int32_t intersectionRight = std::min(
        right(left),
        right(rightRectangle));
    const int32_t intersectionBottom = std::min(
        bottom(left),
        bottom(rightRectangle));
    if (intersectionRight <= intersectionLeft
        || intersectionBottom <= intersectionTop)
        return {};

    return {
        {intersectionLeft, intersectionTop},
        {
            static_cast<uint32_t>(intersectionRight - intersectionLeft),
            static_cast<uint32_t>(intersectionBottom - intersectionTop)
        }
    };
}

Rect LayoutCalculator::unite(const Rect& left, const Rect& rightRectangle)
{
    if (left.size.width == 0U || left.size.height == 0U)
        return rightRectangle;
    if (rightRectangle.size.width == 0U || rightRectangle.size.height == 0U)
        return left;

    const int32_t unionLeft = std::min(left.position.x, rightRectangle.position.x);
    const int32_t unionTop = std::min(left.position.y, rightRectangle.position.y);
    const int32_t unionRight = std::max(right(left), right(rightRectangle));
    const int32_t unionBottom = std::max(bottom(left), bottom(rightRectangle));
    return {
        {unionLeft, unionTop},
        {
            static_cast<uint32_t>(unionRight - unionLeft),
            static_cast<uint32_t>(unionBottom - unionTop)
        }
    };
}

const Container* LayoutCalculator::findCalculatedContainer(
    const Container* const containers,
    const uint32_t containerCount,
    const ContainerId id,
    const bool* const calculated)
{
    if (id == rootContainerId)
        return nullptr;

    for (uint32_t index = 0U; index < containerCount; ++index)
        if (containers[index].id == id && calculated[index])
            return &containers[index];
    return nullptr;
}

void LayoutCalculator::place(
    Container& container,
    const Rect& referenceBounds)
{
    int32_t x = referenceBounds.position.x + container.offset.horizontal;
    int32_t y = referenceBounds.position.y + container.offset.vertical;

    if (container.anchor.horizontal == HorizontalAnchor::CENTER)
        x = referenceBounds.position.x
          + (static_cast<int32_t>(referenceBounds.size.width)
             - static_cast<int32_t>(container.size.width)) / 2
          + container.offset.horizontal;
    else if (container.anchor.horizontal == HorizontalAnchor::RIGHT)
        x = right(referenceBounds)
          - static_cast<int32_t>(container.size.width)
          - container.offset.horizontal;
    else if (container.anchor.horizontal == HorizontalAnchor::BEFORE)
        x = referenceBounds.position.x
          - static_cast<int32_t>(container.size.width)
          - container.offset.horizontal;
    else if (container.anchor.horizontal == HorizontalAnchor::AFTER)
        x = right(referenceBounds) + container.offset.horizontal;
    if (container.anchor.vertical == VerticalAnchor::CENTER)
        y = referenceBounds.position.y
          + (static_cast<int32_t>(referenceBounds.size.height)
             - static_cast<int32_t>(container.size.height)) / 2
          + container.offset.vertical;
    else if (container.anchor.vertical == VerticalAnchor::BOTTOM)
        y = bottom(referenceBounds)
          - static_cast<int32_t>(container.size.height)
          - container.offset.vertical;
    else if (container.anchor.vertical == VerticalAnchor::ABOVE)
        y = referenceBounds.position.y
          - static_cast<int32_t>(container.size.height)
          - container.offset.vertical;
    else if (container.anchor.vertical == VerticalAnchor::BELOW)
        y = bottom(referenceBounds) + container.offset.vertical;

    container.calculatedBounds = {{x, y}, container.size};
}

void LayoutCalculator::displaceFrom(
    Container& container,
    const Container& obstacle)
{
    if (container.displacement == DisplacementDirection::HORIZONTAL)
    {
        container.calculatedBounds.position.x =
            container.anchor.horizontal == HorizontalAnchor::RIGHT
                ? obstacle.calculatedBounds.position.x
                  - static_cast<int32_t>(container.size.width)
                : right(obstacle.calculatedBounds);
        return;
    }

    container.calculatedBounds.position.y =
        container.anchor.vertical == VerticalAnchor::BOTTOM
            ? obstacle.calculatedBounds.position.y
              - static_cast<int32_t>(container.size.height)
            : bottom(obstacle.calculatedBounds);
}
