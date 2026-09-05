// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LoopbackItem.g.h"

#include "AppSettings.h"
#include "StringResources.h"

#include <chrono>

// The pages refresh from the service on a timer, so the row raises property changed rather
// than being replaced. Nothing here throws: a failing notification must never take down a
// UI callback.
#define MIDI_LOOPSETUP_OBSERVABLE_ITEM()                                                       \
    public:                                                                                    \
        winrt::event_token PropertyChanged(                                                    \
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)      \
        {                                                                                      \
            return m_propertyChanged.add(handler);                                             \
        }                                                                                      \
                                                                                               \
        void PropertyChanged(winrt::event_token const& token) noexcept                         \
        {                                                                                      \
            m_propertyChanged.remove(token);                                                   \
        }                                                                                      \
                                                                                               \
    private:                                                                                   \
        void RaisePropertyChanged(std::wstring_view const name) noexcept                       \
        {                                                                                      \
            try                                                                                \
            {                                                                                  \
                m_propertyChanged(                                                             \
                    *this,                                                                     \
                    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ name });       \
            }                                                                                  \
            catch (...)                                                                        \
            {                                                                                  \
            }                                                                                  \
        }                                                                                      \
                                                                                               \
        template <typename TValue>                                                             \
        bool UpdateField(TValue& field, TValue const& value, std::wstring_view const name) noexcept \
        {                                                                                      \
            if (field == value)                                                                \
            {                                                                                  \
                return false;                                                                  \
            }                                                                                  \
                                                                                               \
            field = value;                                                                     \
            RaisePropertyChanged(name);                                                        \
                                                                                               \
            return true;                                                                       \
        }                                                                                      \
                                                                                               \
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};

namespace midiloopbacksetup
{
    // Everything a row shows, gathered in one place so a refresh is a single call rather than
    // a dozen setters which each raise a change notification.
    struct LoopbackRowData
    {
        winrt::hstring AssociationId{};

        winrt::hstring DisplayName{};

        winrt::hstring NameA{};
        winrt::hstring DescriptionA{};
        winrt::hstring EndpointDeviceIdA{};

        winrt::hstring NameB{};
        winrt::hstring DescriptionB{};
        winrt::hstring EndpointDeviceIdB{};

        // bare file name in the shared endpoint assets folder
        winrt::hstring ImageFileName{};

        // localized, because the row does not reach into the resource loader itself
        winrt::hstring MuteButtonLabel{};
        winrt::hstring PersistenceText{};

        // Every row carries a mute and a delete button, so each needs a name which says which
        // loopback it acts on. "Delete" on its own is the same name eleven times over.
        winrt::hstring MuteButtonAccessibleName{};
        winrt::hstring DeleteButtonAccessibleName{};
        winrt::hstring EditButtonAccessibleName{};

        bool HasSecondEndpoint{ false };
        bool IsMuted{ false };
        bool IsPersisted{ false };

        // running total the service reports, and whether it reported one at all: the MIDI 2.0
        // loopback transport has no counter, so its rows must not draw an empty graph
        uint64_t MessageCount{ 0 };
        bool HasMessageCount{ false };
    };
}

namespace winrt::midiloopbacksetup::implementation
{
    // Message traffic over a rolling window, rendered as a filled sparkline.
    //
    // The service reports a running total, so what is plotted is the difference between polls
    // divided by the time between them. A total would climb forever and say nothing about what
    // is happening now.
    //
    // The x axis is real elapsed time rather than one step per sample, because a refresh is also
    // requested after a create, a delete and a mute, and a tick is skipped while the previous
    // refresh is still running.
    struct TrafficHistory
    {
        // how many polls' worth of history the window covers
        static constexpr size_t SampleCapacity = 30;

        // hard ceiling, in case something requests refreshes far faster than the poll interval
        static constexpr size_t MaxSamples = 400;

        winrt::Microsoft::UI::Xaml::Media::PointCollection LinePoints{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::PointCollection FillPoints{ nullptr };
        winrt::hstring TrafficText{};

        void Record(
            _In_ uint64_t const totalMessages,
            _In_ double const graphWidth,
            _In_ double const graphHeight) noexcept
        {
            auto const now = Clock::now();

            // A count which went backwards means the endpoint was torn down and rebuilt, so the
            // old readings describe something which no longer exists.
            if (totalMessages < m_lastTotal)
            {
                m_samples.clear();
                m_hasLastTotal = false;
            }

            auto const elapsed = m_hasLastTotal ?
                std::chrono::duration<double>{ now - m_lastSampleTime }.count() : 0.0;

            // The first reading has nothing to subtract from, so it starts the trace at rest
            // rather than plotting the whole lifetime of the endpoint as one enormous spike.
            auto const rate = (m_hasLastTotal && elapsed > 0.0) ?
                static_cast<double>(totalMessages - m_lastTotal) / elapsed : 0.0;

            m_lastTotal = totalMessages;
            m_lastSampleTime = now;
            m_hasLastTotal = true;

            m_samples.push_back({ now, rate });

            auto const window = std::chrono::duration<double>{ WindowSeconds() };

            while (m_samples.size() > 1 && (now - m_samples.front().first) > window)
            {
                m_samples.pop_front();
            }

            while (m_samples.size() > MaxSamples)
            {
                m_samples.pop_front();
            }

            Rebuild(now, totalMessages, graphWidth, graphHeight);
        }

    private:
        using Clock = std::chrono::steady_clock;

        static double WindowSeconds() noexcept
        {
            auto const interval = ::midiloopbacksetup::AppSettings::Current().RefreshIntervalSeconds();

            return static_cast<double>(SampleCapacity) *
                static_cast<double>(interval == 0 ? 1 : interval);
        }

        void Rebuild(
            _In_ Clock::time_point const now,
            _In_ uint64_t const totalMessages,
            _In_ double const graphWidth,
            _In_ double const graphHeight) noexcept
        {
            try
            {
                winrt::Microsoft::UI::Xaml::Media::PointCollection line{};
                winrt::Microsoft::UI::Xaml::Media::PointCollection fill{};

                TrafficText = ::midiloopbacksetup::resources::FormatString(
                    L"LoopbackTrafficFormat", totalMessages);

                if (m_samples.empty())
                {
                    LinePoints = line;
                    FillPoints = fill;

                    return;
                }

                auto const peak = std::max_element(
                    m_samples.begin(),
                    m_samples.end(),
                    [](auto const& left, auto const& right) { return left.second < right.second; })->second;

                // A flat trace pinned to the very top would suggest the scale means something
                // absolute, so the axis keeps a little headroom above the peak.
                auto const scale = peak > 0.0 ? peak * 1.15 : 1.0;

                // Logarithmic, because one system exclusive dump or a held chord is orders of
                // magnitude above ordinary playing and would otherwise flatten everything else
                // onto the baseline. log1p keeps an idle loopback at the baseline.
                auto const scaleLog = std::log1p(scale);

                auto const window = WindowSeconds();

                // The stroke is centered on the point, so an idle loopback drawn exactly on the
                // baseline would have half its line clipped by the border it sits in.
                auto const inset = 1.0;
                auto const usableHeight = graphHeight - (inset * 2.0);

                for (auto const& sample : m_samples)
                {
                    auto const age = std::chrono::duration<double>{ now - sample.first }.count();

                    auto x = graphWidth - ((age / window) * graphWidth);

                    x = std::clamp(x, 0.0, graphWidth);

                    auto const normalized = scaleLog > 0.0 ?
                        std::clamp(std::log1p(sample.second) / scaleLog, 0.0, 1.0) : 0.0;

                    auto const y = inset + usableHeight - (normalized * usableHeight);

                    line.Append(winrt::Windows::Foundation::Point{
                        static_cast<float>(x), static_cast<float>(y) });
                }

                for (auto const& point : line)
                {
                    fill.Append(point);
                }

                // close the area down to the baseline so the polygon fills under the trace
                fill.Append(winrt::Windows::Foundation::Point{
                    static_cast<float>(line.GetAt(line.Size() - 1).X), static_cast<float>(graphHeight) });

                fill.Append(winrt::Windows::Foundation::Point{
                    static_cast<float>(line.GetAt(0).X), static_cast<float>(graphHeight) });

                LinePoints = line;
                FillPoints = fill;
            }
            catch (...)
            {
            }
        }

        std::deque<std::pair<Clock::time_point, double>> m_samples{};

        uint64_t m_lastTotal{ 0 };
        Clock::time_point m_lastSampleTime{};
        bool m_hasLastTotal{ false };
    };
    struct LoopbackItem : LoopbackItemT<LoopbackItem>
    {
        LoopbackItem() = default;

        winrt::hstring AssociationId() const noexcept { return m_associationId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }

        winrt::hstring NameA() const noexcept { return m_nameA; }
        winrt::hstring DescriptionA() const noexcept { return m_descriptionA; }
        winrt::hstring EndpointDeviceIdA() const noexcept { return m_endpointDeviceIdA; }

        winrt::hstring NameB() const noexcept { return m_nameB; }
        winrt::hstring DescriptionB() const noexcept { return m_descriptionB; }
        winrt::hstring EndpointDeviceIdB() const noexcept { return m_endpointDeviceIdB; }

        bool HasSecondEndpoint() const noexcept { return m_hasSecondEndpoint; }

        xaml::Visibility SecondEndpointVisibility() const noexcept
        {
            return m_hasSecondEndpoint ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility DescriptionAVisibility() const noexcept
        {
            return m_descriptionA.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility DescriptionBVisibility() const noexcept
        {
            return (m_hasSecondEndpoint && !m_descriptionB.empty()) ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }

        xaml::Media::ImageSource ImageSource() const noexcept { return m_imageSource; }

        xaml::Visibility ImageVisibility() const noexcept
        {
            return m_imageSource != nullptr ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility GlyphVisibility() const noexcept
        {
            return m_imageSource != nullptr ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        bool IsMuted() const noexcept { return m_isMuted; }

        winrt::hstring MuteButtonLabel() const noexcept { return m_muteButtonLabel; }

        winrt::hstring MuteButtonAccessibleName() const noexcept { return m_muteButtonAccessibleName; }
        winrt::hstring DeleteButtonAccessibleName() const noexcept { return m_deleteButtonAccessibleName; }
        winrt::hstring EditButtonAccessibleName() const noexcept { return m_editButtonAccessibleName; }

        // Speaker vs. muted speaker. The glyph carries the state at a glance; the label says
        // what the button will do.
        winrt::hstring MuteButtonGlyph() const noexcept
        {
            return m_isMuted ? winrt::hstring{ L"\xE74F" } : winrt::hstring{ L"\xE767" };
        }

        xaml::Visibility MutedBadgeVisibility() const noexcept
        {
            return m_isMuted ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        bool CanMute() const noexcept { return m_canMute; }
        void CanMute(bool const value) noexcept
        {
            if (UpdateField(m_canMute, value, L"CanMute"))
            {
                RaisePropertyChanged(L"MuteVisibility");
            }
        }

        xaml::Visibility MuteVisibility() const noexcept
        {
            return m_canMute ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        // The transport has to say it can customize an endpoint before the row offers to. A
        // rolled back service reports false, and an edit that silently did nothing would be
        // worse than no button at all.
        bool CanCustomize() const noexcept { return m_canCustomize; }
        void CanCustomize(bool const value) noexcept
        {
            if (UpdateField(m_canCustomize, value, L"CanCustomize"))
            {
                RaisePropertyChanged(L"EditVisibility");
            }
        }

        xaml::Visibility EditVisibility() const noexcept
        {
            return m_canCustomize ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        bool IsPersisted() const noexcept { return m_isPersisted; }
        void IsPersisted(bool const value) noexcept { UpdateField(m_isPersisted, value, L"IsPersisted"); }

        // Traffic history. The geometry is generated against a fixed box, so the sparkline in
        // the data template has to be that size.
        winrt::Microsoft::UI::Xaml::Media::PointCollection TrafficLinePoints() const noexcept { return m_traffic.LinePoints; }
        winrt::Microsoft::UI::Xaml::Media::PointCollection TrafficFillPoints() const noexcept { return m_traffic.FillPoints; }
        winrt::hstring TrafficText() const noexcept { return m_traffic.TrafficText; }

        xaml::Visibility TrafficVisibility() const noexcept
        {
            return m_hasMessageCount ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        winrt::hstring PersistenceText() const noexcept { return m_persistenceText; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        int32_t DisplayOrder() const noexcept { return m_displayOrder; }
        void DisplayOrder(int32_t const value) noexcept { UpdateField(m_displayOrder, value, L"DisplayOrder"); }

        void InternalInitialize(_In_ winrt::hstring const& associationId) noexcept
        {
            m_associationId = associationId;
        }

        void InternalUpdate(_In_ ::midiloopbacksetup::LoopbackRowData const& data) noexcept
        {
            UpdateField(m_displayName, data.DisplayName, L"DisplayName");

            if (UpdateField(m_nameA, data.NameA, L"NameA") ||
                UpdateField(m_descriptionA, data.DescriptionA, L"DescriptionA"))
            {
                RaisePropertyChanged(L"DescriptionAVisibility");
            }

            UpdateField(m_endpointDeviceIdA, data.EndpointDeviceIdA, L"EndpointDeviceIdA");

            if (UpdateField(m_nameB, data.NameB, L"NameB") ||
                UpdateField(m_descriptionB, data.DescriptionB, L"DescriptionB"))
            {
                RaisePropertyChanged(L"DescriptionBVisibility");
            }

            UpdateField(m_endpointDeviceIdB, data.EndpointDeviceIdB, L"EndpointDeviceIdB");

            if (UpdateField(m_imageFileName, data.ImageFileName, L"ImageFileName"))
            {
                RefreshImageSource();
            }

            if (UpdateField(m_hasSecondEndpoint, data.HasSecondEndpoint, L"HasSecondEndpoint"))
            {
                RaisePropertyChanged(L"SecondEndpointVisibility");
                RaisePropertyChanged(L"DescriptionBVisibility");
            }

            if (UpdateField(m_isMuted, data.IsMuted, L"IsMuted"))
            {
                RaisePropertyChanged(L"MuteButtonGlyph");
                RaisePropertyChanged(L"MutedBadgeVisibility");
            }

            UpdateField(m_muteButtonLabel, data.MuteButtonLabel, L"MuteButtonLabel");
            UpdateField(m_muteButtonAccessibleName, data.MuteButtonAccessibleName, L"MuteButtonAccessibleName");
            UpdateField(m_deleteButtonAccessibleName, data.DeleteButtonAccessibleName, L"DeleteButtonAccessibleName");
            UpdateField(m_editButtonAccessibleName, data.EditButtonAccessibleName, L"EditButtonAccessibleName");
            UpdateField(m_persistenceText, data.PersistenceText, L"PersistenceText");
            UpdateField(m_isPersisted, data.IsPersisted, L"IsPersisted");

            if (UpdateField(m_hasMessageCount, data.HasMessageCount, L"HasMessageCount"))
            {
                RaisePropertyChanged(L"TrafficVisibility");
            }

            if (m_hasMessageCount)
            {
                RecordTrafficSample(data.MessageCount);
            }
        }

    private:
        // must match the sparkline's size in the basic loopback item template in MainWindow.xaml
        static constexpr double TrafficGraphWidth = 140.0;
        static constexpr double TrafficGraphHeight = 28.0;

        void RecordTrafficSample(_In_ uint64_t const totalMessages) noexcept
        {
            m_traffic.Record(totalMessages, TrafficGraphWidth, TrafficGraphHeight);

            RaisePropertyChanged(L"TrafficLinePoints");
            RaisePropertyChanged(L"TrafficFillPoints");
            RaisePropertyChanged(L"TrafficText");
        }

        // The picture is loaded once per change rather than per redraw, and a name which no
        // longer resolves to a file simply leaves the glyph in place.
        void RefreshImageSource() noexcept
        {
            m_imageSource = nullptr;

            if (!m_imageFileName.empty())
            {
                auto const path = midiapp::EndpointImageAssets::FullPathForFileName(
                    std::wstring{ m_imageFileName });

                if (!path.empty() && midiapp::EndpointImageAssets::Exists(std::wstring{ m_imageFileName }))
                {
                    try
                    {
                        foundation::Uri const uri{ winrt::hstring{ path } };

                        if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ m_imageFileName }))
                        {
                            m_imageSource = xaml::Media::Imaging::SvgImageSource{ uri };
                        }
                        else
                        {
                            m_imageSource = xaml::Media::Imaging::BitmapImage{ uri };
                        }
                    }
                    catch (...)
                    {
                        m_imageSource = nullptr;
                    }
                }
            }

            RaisePropertyChanged(L"ImageSource");
            RaisePropertyChanged(L"ImageVisibility");
            RaisePropertyChanged(L"GlyphVisibility");
        }

        winrt::hstring m_associationId{};
        winrt::hstring m_displayName{};

        winrt::hstring m_nameA{};
        winrt::hstring m_descriptionA{};
        winrt::hstring m_endpointDeviceIdA{};

        winrt::hstring m_nameB{};
        winrt::hstring m_descriptionB{};
        winrt::hstring m_endpointDeviceIdB{};

        winrt::hstring m_muteButtonLabel{};
        winrt::hstring m_muteButtonAccessibleName{};
        winrt::hstring m_deleteButtonAccessibleName{};
        winrt::hstring m_editButtonAccessibleName{};
        winrt::hstring m_persistenceText{};

        winrt::hstring m_imageFileName{};
        xaml::Media::ImageSource m_imageSource{ nullptr };

        bool m_hasSecondEndpoint{ false };
        bool m_isMuted{ false };
        bool m_canMute{ false };
        bool m_canCustomize{ false };
        bool m_isPersisted{ false };
        bool m_isBusy{ false };
        bool m_hasMessageCount{ false };

        int32_t m_displayOrder{ 0 };

        TrafficHistory m_traffic{};

        MIDI_LOOPSETUP_OBSERVABLE_ITEM()
    };
}

namespace winrt::midiloopbacksetup::factory_implementation
{
    struct LoopbackItem : LoopbackItemT<LoopbackItem, implementation::LoopbackItem>
    {
    };
}
