// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "WrapPanel.h"
#include "WrapPanel.g.cpp"

#include <limits>

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr float Gap = 6.0f;

        // Calls place(child, x, y) for each visible child and returns the size they cover.
        template <typename Place>
        foundation::Size Flow(xaml::Controls::UIElementCollection const& children, float width, Place&& place)
        {
            float x{ 0 };
            float y{ 0 };
            float rowHeight{ 0 };
            float used{ 0 };

            for (auto const& child : children)
            {
                auto const size = child.DesiredSize();

                if (child.Visibility() != xaml::Visibility::Visible || (size.Width <= 0 && size.Height <= 0))
                {
                    continue;
                }

                if (x > 0 && x + size.Width > width)
                {
                    y += rowHeight + Gap;
                    x = 0;
                    rowHeight = 0;
                }

                place(child, x, y);

                x += size.Width;
                used = std::max(used, x);
                x += Gap;
                rowHeight = std::max(rowHeight, size.Height);
            }

            return foundation::Size{ used, y + rowHeight };
        }
    }

    _Use_decl_annotations_
    foundation::Size WrapPanel::MeasureOverride(foundation::Size const& availableSize)
    {
        auto const children = Children();

        for (auto const& child : children)
        {
            child.Measure(foundation::Size{ availableSize.Width, std::numeric_limits<float>::infinity() });
        }

        return Flow(children, availableSize.Width, [](auto&&, float, float) {});
    }

    _Use_decl_annotations_
    foundation::Size WrapPanel::ArrangeOverride(foundation::Size const& finalSize)
    {
        auto const children = Children();

        for (auto const& child : children)
        {
            if (child.Visibility() != xaml::Visibility::Visible)
            {
                child.Arrange(foundation::Rect{ 0, 0, 0, 0 });
            }
        }

        Flow(children, finalSize.Width, [](xaml::UIElement const& child, float x, float y)
        {
            auto const size = child.DesiredSize();
            child.Arrange(foundation::Rect{ x, y, size.Width, size.Height });
        });

        return finalSize;
    }
}
