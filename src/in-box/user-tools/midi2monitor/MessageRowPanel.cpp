// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageRowPanel.h"
#include "MessageRowPanel.g.cpp"

namespace midi2monitor
{
    namespace
    {
        std::vector<ColumnDisplayEntry> g_entries{};
        bool g_autoHideWhenNarrow{ true };
    }

    std::vector<ColumnDisplayEntry>& ColumnLayoutState::Entries() noexcept
    {
        if (g_entries.empty())
        {
            ResetToDefault();
        }

        return g_entries;
    }

    void ColumnLayoutState::ResetToDefault() noexcept
    {
        g_entries.clear();

        for (int32_t i = 0; i < MonitorColumnCount; i++)
        {
            g_entries.push_back(ColumnDisplayEntry{ static_cast<MonitorColumnId>(i), true });
        }
    }

    bool ColumnLayoutState::AutoHideWhenNarrow() noexcept
    {
        return g_autoHideWhenNarrow;
    }

    _Use_decl_annotations_
    void ColumnLayoutState::AutoHideWhenNarrow(bool value) noexcept
    {
        g_autoHideWhenNarrow = value;
    }

    std::wstring ColumnLayoutState::Serialize()
    {
        std::wstring result{};

        for (auto const& entry : Entries())
        {
            if (!result.empty())
            {
                result.append(L",");
            }

            if (!entry.IsVisible)
            {
                result.append(L"-");
            }

            result.append(std::format(L"{}", static_cast<int32_t>(entry.Id)));
        }

        return result;
    }

    _Use_decl_annotations_
    void ColumnLayoutState::Deserialize(std::wstring const& value)
    {
        if (value.empty())
        {
            ResetToDefault();
            return;
        }

        std::vector<ColumnDisplayEntry> parsed{};
        std::array<bool, MonitorColumnCount> seen{};

        size_t position{ 0 };

        while (position <= value.size())
        {
            auto const separator = value.find(L',', position);
            auto const token = value.substr(position, separator == std::wstring::npos ? std::wstring::npos : separator - position);

            if (!token.empty())
            {
                bool visible{ true };
                std::wstring digits{ token };

                if (digits.front() == L'-')
                {
                    visible = false;
                    digits.erase(0, 1);
                }

                int32_t id{ -1 };

                if (!digits.empty() && std::all_of(digits.begin(), digits.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }))
                {
                    id = std::stoi(digits);
                }

                if (id >= 0 && id < MonitorColumnCount && !seen[static_cast<size_t>(id)])
                {
                    seen[static_cast<size_t>(id)] = true;
                    parsed.push_back(ColumnDisplayEntry{ static_cast<MonitorColumnId>(id), visible });
                }
            }

            if (separator == std::wstring::npos)
            {
                break;
            }

            position = separator + 1;
        }

        // anything the stored layout did not mention keeps its default position at the end
        for (int32_t i = 0; i < MonitorColumnCount; i++)
        {
            if (!seen[static_cast<size_t>(i)])
            {
                parsed.push_back(ColumnDisplayEntry{ static_cast<MonitorColumnId>(i), true });
            }
        }

        g_entries = std::move(parsed);
    }
}

namespace winrt::midi2monitor::implementation
{
    namespace native = ::midi2monitor;

    _Use_decl_annotations_
    foundation::Size MessageRowPanel::MeasureOverride(foundation::Size const& availableSize)
    {
        m_placement.clear();

        auto const children = Children();

        if (children.Size() == 0)
        {
            return foundation::Size{ 0, 0 };
        }

        // child 0 is the comment gutter, children 1..n are the columns in canonical order
        auto const columnChildCount = children.Size() - 1;

        struct Candidate
        {
            native::MonitorColumnId Id;
            uint32_t ChildIndex;
            native::MonitorColumnMetrics Metrics;
        };

        std::vector<Candidate> candidates{};

        for (auto const& entry : native::ColumnLayoutState::Entries())
        {
            auto const childIndex = static_cast<uint32_t>(entry.Id) + 1;

            if (!entry.IsVisible || childIndex > columnChildCount)
            {
                continue;
            }

            candidates.push_back(Candidate{ entry.Id, childIndex, native::GetColumnMetrics(entry.Id) });
        }

        double const gutterWidth = native::CommentGutterWidth;

        double available = availableSize.Width;

        if (!std::isfinite(available) || available <= 0.0)
        {
            available = gutterWidth;

            for (auto const& candidate : candidates)
            {
                available += candidate.Metrics.Width;
            }
        }

        double contentWidth = std::max(0.0, available - gutterWidth);

        if (native::ColumnLayoutState::AutoHideWhenNarrow())
        {
            // drop the least important columns until what remains fits
            for (;;)
            {
                double required{ 0.0 };

                for (auto const& candidate : candidates)
                {
                    required += candidate.Metrics.Width;
                }

                if (required <= contentWidth || candidates.empty())
                {
                    break;
                }

                auto lowest = candidates.end();

                for (auto it = candidates.begin(); it != candidates.end(); ++it)
                {
                    if (it->Metrics.DropPriority < 0)
                    {
                        continue;
                    }

                    if (lowest == candidates.end() || it->Metrics.DropPriority < lowest->Metrics.DropPriority)
                    {
                        lowest = it;
                    }
                }

                if (lowest == candidates.end())
                {
                    break;
                }

                candidates.erase(lowest);
            }
        }

        double fixedWidth{ 0.0 };
        size_t starCount{ 0 };

        for (auto const& candidate : candidates)
        {
            if (candidate.Metrics.IsStar)
            {
                starCount++;
            }
            else
            {
                fixedWidth += candidate.Metrics.Width;
            }
        }

        double starWidth{ 0.0 };

        if (starCount > 0)
        {
            starWidth = std::max(
                (contentWidth - fixedWidth) / static_cast<double>(starCount),
                native::GetColumnMetrics(native::MonitorColumnId::Decoded).Width);
        }

        // hide every child first, then place the ones that survived
        for (uint32_t i = 0; i < children.Size(); i++)
        {
            children.GetAt(i).Measure(foundation::Size{ 0, 0 });
        }

        double rowHeight{ 0.0 };
        double offset{ gutterWidth };

        auto gutter = children.GetAt(0);
        gutter.Measure(foundation::Size{ static_cast<float>(gutterWidth), availableSize.Height });
        rowHeight = std::max(rowHeight, static_cast<double>(gutter.DesiredSize().Height));

        m_placement.push_back(PlacedColumn{ 0, 0.0, gutterWidth });

        for (auto const& candidate : candidates)
        {
            auto const width = candidate.Metrics.IsStar ? starWidth : candidate.Metrics.Width;

            auto child = children.GetAt(candidate.ChildIndex);
            child.Measure(foundation::Size{ static_cast<float>(width), std::numeric_limits<float>::infinity() });

            rowHeight = std::max(rowHeight, static_cast<double>(child.DesiredSize().Height));

            m_placement.push_back(PlacedColumn{ candidate.ChildIndex, offset, width });

            offset += width;
        }

        return foundation::Size{ static_cast<float>(offset), static_cast<float>(rowHeight) };
    }

    _Use_decl_annotations_
    foundation::Size MessageRowPanel::ArrangeOverride(foundation::Size const& finalSize)
    {
        auto const children = Children();

        // anything not in the placement list is arranged to zero size, which keeps it out of
        // both layout and the accessibility tree
        std::vector<bool> placed(children.Size(), false);

        for (auto const& placement : m_placement)
        {
            if (placement.ChildIndex >= children.Size())
            {
                continue;
            }

            placed[placement.ChildIndex] = true;

            children.GetAt(placement.ChildIndex).Arrange(foundation::Rect{
                static_cast<float>(placement.Offset),
                0.0f,
                static_cast<float>(placement.Width),
                finalSize.Height });
        }

        for (uint32_t i = 0; i < children.Size(); i++)
        {
            if (!placed[i])
            {
                children.GetAt(i).Arrange(foundation::Rect{ 0.0f, 0.0f, 0.0f, 0.0f });
            }
        }

        return finalSize;
    }
}
