// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClockItems.h"
#include "ClockItem.g.cpp"
#include "StringResources.h"

namespace res = ::midiclock::resources;

namespace winrt::midiclock::implementation
{
    namespace
    {
        winrt::hstring FormatTempo(_In_ double const beatsPerMinute) noexcept
        {
            try
            {
                auto const rounded = std::round(beatsPerMinute * 10.0) / 10.0;

                // whole tempos are the common case and read better without a trailing zero
                if (std::abs(rounded - std::round(rounded)) < 0.05)
                {
                    return winrt::hstring{ std::format(L"{:.0f}", std::round(rounded)) };
                }

                return winrt::hstring{ std::format(L"{:.1f}", rounded) };
            }
            catch (...)
            {
                return L"";
            }
        }

        winrt::hstring DescribeDestination(
            _In_ std::wstring const& endpointName,
            _In_ int32_t const groupIndex) noexcept
        {
            if (endpointName.empty())
            {
                return res::GetString(L"TileNoEndpoint");
            }

            if (groupIndex == ::midiclock::AllDeclaredGroups)
            {
                return res::FormatString(L"TileDestinationAllGroupsFormat", endpointName);
            }

            return res::FormatString(L"TileDestinationFormat", endpointName, groupIndex + 1);
        }

        // Only the parts that are not at their default, so a plain clock says nothing extra and
        // the tile keeps its usual height.
        winrt::hstring DescribeTiming(
            _In_ ::midiclock::ClockRowData const& data,
            _In_ std::optional<midiapp::MidiTimeCodePosition> const& playingPosition) noexcept
        {
            try
            {
                std::wstring text{};

                auto const append = [&text](winrt::hstring const& part)
                    {
                        if (part.empty())
                        {
                            return;
                        }

                        if (!text.empty())
                        {
                            text.append(L"  \u00b7  ");
                        }

                        text.append(part);
                    };

                if (data.Kind == ::midiclock::ClockKind::TimeCode)
                {
                    // A time that never moves would look like a broken counter, so none at all.
                    if (data.ShowTimeCode)
                    {
                        append(winrt::hstring{ midiapp::FormatPosition(
                            playingPosition.value_or(data.StartTimeCode), data.FrameRate) });
                    }
                }
                else
                {
                    if (data.ClockRatioNumerator != data.ClockRatioDenominator)
                    {
                        auto const effective = data.BeatsPerMinute *
                            static_cast<double>(data.ClockRatioNumerator) /
                            static_cast<double>(data.ClockRatioDenominator);

                        append(res::FormatString(L"TileRatioFormat",
                            data.ClockRatioNumerator, data.ClockRatioDenominator, FormatTempo(effective)));
                    }

                    if (data.SwingPercent > ::midiclock::MinimumSwingPercent)
                    {
                        append(res::FormatString(L"TileSwingFormat",
                            std::format(L"{:.0f}", std::round(data.SwingPercent))));
                    }
                }

                if (data.OffsetMilliseconds != 0.0)
                {
                    append(res::FormatString(L"TileOffsetFormat",
                        std::format(L"{:+.0f}", data.OffsetMilliseconds)));
                }

                return winrt::hstring{ text };
            }
            catch (...)
            {
                return L"";
            }
        }
    }

    _Use_decl_annotations_
    void ClockItem::Update(::midiclock::ClockRowData const& data) noexcept
    {
        try
        {
            m_data = data;

            if (m_id != winrt::hstring{ data.Id })
            {
                m_id = winrt::hstring{ data.Id };
                RaisePropertyChanged(L"Id");
            }

            auto const displayName = winrt::hstring{ data.DisplayName };

            if (m_displayName != displayName)
            {
                m_displayName = displayName;
                RaisePropertyChanged(L"DisplayName");
                RaisePropertyChanged(L"StartStopAccessibleName");
                RaisePropertyChanged(L"EditAccessibleName");
                RaisePropertyChanged(L"SelectAccessibleName");
            }

            auto const tempoText = data.Kind == ::midiclock::ClockKind::TimeCode
                ? winrt::hstring{ midiapp::FrameRateShortName(data.FrameRate) }
                : FormatTempo(data.BeatsPerMinute);

            if (m_tempoText != tempoText)
            {
                m_tempoText = tempoText;
                RaisePropertyChanged(L"TempoText");
            }

            auto const unitText = res::GetString(
                data.Kind == ::midiclock::ClockKind::TimeCode ? L"TileFramesPerSecondCaption" : L"TileBpmCaption");

            if (m_unitText != unitText)
            {
                m_unitText = unitText;
                RaisePropertyChanged(L"UnitText");
            }

            auto const destinationText = DescribeDestination(data.EndpointName, data.GroupIndex);

            if (m_destinationText != destinationText)
            {
                m_destinationText = destinationText;
                RaisePropertyChanged(L"DestinationText");
            }

            RefreshTimingText();

            if (m_isEndpointMissing != data.IsEndpointMissing)
            {
                m_isEndpointMissing = data.IsEndpointMissing;
                RaisePropertyChanged(L"IsEndpointMissing");
                RaisePropertyChanged(L"WarningVisibility");
                RaisePropertyChanged(L"StatusText");
            }

            RaisePropertyChanged(L"TileAccessibleName");
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update a clock tile.")
    }

    void ClockItem::RefreshTimingText() noexcept
    {
        try
        {
            auto timingText = DescribeTiming(m_data, m_playingPosition);

            if (m_timingText == timingText)
            {
                return;
            }

            auto const visibilityChanged = m_timingText.empty() != timingText.empty();

            m_timingText = std::move(timingText);

            RaisePropertyChanged(L"TimingText");

            if (visibilityChanged)
            {
                RaisePropertyChanged(L"TimingVisibility");
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update the timing line on a clock tile.")
    }

    _Use_decl_annotations_
    void ClockItem::PlayingPosition(midiapp::MidiTimeCodePosition const& position) noexcept
    {
        m_playingPosition = position;

        RefreshTimingText();
    }

    void ClockItem::RaiseRunStateChanged() noexcept
    {
        RaisePropertyChanged(L"IsRunning");
        RaisePropertyChanged(L"RunningVisibility");
        RaisePropertyChanged(L"StartStopLabel");
        RaisePropertyChanged(L"StartStopGlyph");
        RaisePropertyChanged(L"StartStopAccessibleName");
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"TileAccessibleName");
    }

    _Use_decl_annotations_
    void ClockItem::IsRunning(bool value) noexcept
    {
        if (m_isRunning == value)
        {
            return;
        }

        m_isRunning = value;

        // back to the time the next start begins from
        if (!m_isRunning && m_playingPosition.has_value())
        {
            m_playingPosition.reset();
            RefreshTimingText();
        }

        RaiseRunStateChanged();
    }

    _Use_decl_annotations_
    void ClockItem::IsBusy(bool value) noexcept
    {
        if (m_isBusy == value)
        {
            return;
        }

        m_isBusy = value;

        RaisePropertyChanged(L"IsBusy");
        RaisePropertyChanged(L"IsStartStopEnabled");
        RaisePropertyChanged(L"StatusText");
        RaisePropertyChanged(L"TileAccessibleName");
    }

    _Use_decl_annotations_
    void ClockItem::IsSelected(bool value) noexcept
    {
        if (m_isSelected == value)
        {
            return;
        }

        m_isSelected = value;

        RaisePropertyChanged(L"IsSelected");
    }

    winrt::hstring ClockItem::StatusText() const noexcept
    {
        if (m_isBusy)
        {
            return res::GetString(L"TileStatusWorking");
        }

        if (m_isRunning)
        {
            return res::GetString(L"TileStatusRunning");
        }

        if (m_isEndpointMissing)
        {
            return res::GetString(L"TileStatusEndpointMissing");
        }

        return res::GetString(L"TileStatusStopped");
    }

    winrt::hstring ClockItem::StartStopLabel() const noexcept
    {
        return res::GetString(m_isRunning ? L"TileStopLabel" : L"TileStartLabel");
    }

    winrt::hstring ClockItem::StartStopGlyph() const noexcept
    {
        // Segoe Fluent Icons: Stop, Play
        return m_isRunning ? winrt::hstring{ L"\uE71A" } : winrt::hstring{ L"\uE768" };
    }

    winrt::hstring ClockItem::StartStopAccessibleName() const noexcept
    {
        return res::FormatString(
            m_isRunning ? L"TileStopAccessibleNameFormat" : L"TileStartAccessibleNameFormat",
            m_displayName);
    }

    winrt::hstring ClockItem::EditAccessibleName() const noexcept
    {
        return res::FormatString(L"TileEditAccessibleNameFormat", m_displayName);
    }

    winrt::hstring ClockItem::SelectAccessibleName() const noexcept
    {
        return res::FormatString(L"TileSelectAccessibleNameFormat", m_displayName);
    }

    winrt::hstring ClockItem::TileAccessibleName() const noexcept
    {
        return res::FormatString(
            L"TileAccessibleNameFormat",
            std::wstring{ m_displayName },
            std::wstring{ m_tempoText },
            std::wstring{ m_destinationText },
            std::wstring{ StatusText() });
    }
}
