// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midi2monitor
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so existing call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class TimestampDisplayFormat : int32_t
    {
        Ticks = 0,
        Microseconds = 1,
        Milliseconds = 2,
        Seconds = 3
    };

    // Per-user settings, persisted under HKCU so that every instance of the app picks up the
    // same defaults. Reads and writes never throw; on failure the in-memory value is used.
    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        bool ShowClockMessages() const noexcept { return m_showClockMessages; }
        void ShowClockMessages(bool value) noexcept;

        bool ShowActiveSenseMessages() const noexcept { return m_showActiveSenseMessages; }
        void ShowActiveSenseMessages(bool value) noexcept;

        TimestampDisplayFormat TimestampFormat() const noexcept { return m_timestampFormat; }
        void TimestampFormat(TimestampDisplayFormat value) noexcept;

        uint32_t RetainedMessageCount() const noexcept { return m_retainedMessageCount; }
        void RetainedMessageCount(uint32_t value) noexcept;

        bool ShowMessageNameChiclets() const noexcept { return m_showMessageNameChiclets; }
        void ShowMessageNameChiclets(bool value) noexcept;

        bool AutoHideColumnsWhenNarrow() const noexcept { return m_autoHideColumnsWhenNarrow; }
        void AutoHideColumnsWhenNarrow(bool value) noexcept;

        // comma separated list of column ids, in display order, with a '-' prefix for hidden
        std::wstring ColumnLayout() const noexcept { return m_columnLayout; }
        void ColumnLayout(std::wstring const& value) noexcept;

        uint32_t TableZoomPercent() const noexcept { return m_tableZoomPercent; }
        void TableZoomPercent(uint32_t value) noexcept;

        // Off once someone checks "Don't ask me again" on the clear confirmation.
        bool AskBeforeClear() const noexcept { return m_askBeforeClear; }
        void AskBeforeClear(_In_ bool value) noexcept;

        // What simple mode watched last, so the next start picks it again. Empty and 0 when
        // nothing was picked yet. Group and channel count from 1; 0 means all.
        std::wstring LastEndpointDeviceId() const noexcept { return m_lastEndpointDeviceId; }
        uint8_t LastGroupNumber() const noexcept { return m_lastGroupNumber; }
        uint8_t LastChannelNumber() const noexcept { return m_lastChannelNumber; }
        void LastSimpleSelection(_In_ std::wstring const& endpointDeviceId, _In_ uint8_t groupNumber, _In_ uint8_t channelNumber) noexcept;

        // ---------------- advanced mode ----------------

        bool AdvancedMode() const noexcept { return m_advancedMode; }
        void AdvancedMode(_In_ bool value) noexcept;

        // The groups to listen to, one endpoint a line: the endpoint device id, a tab, and the
        // groups as a hex mask where bit 0 is group 1.
        std::wstring AdvancedSources() const noexcept { return m_advancedSources; }
        void AdvancedSources(_In_ std::wstring const& value) noexcept;

        // The shared message filter as JSON, written and read with FilterToJson and FilterFromJson.
        std::wstring AdvancedFilter() const noexcept { return m_advancedFilter; }
        void AdvancedFilter(_In_ std::wstring const& value) noexcept;

        bool FoldRepeats() const noexcept { return m_foldRepeats; }
        void FoldRepeats(_In_ bool value) noexcept;

        // Opens every endpoint while listening, so the Sources list can say which are sending.
        // Off for a PC with so many endpoints that opening them all is slow.
        bool ShowEndpointActivity() const noexcept { return m_showEndpointActivity; }
        void ShowEndpointActivity(_In_ bool value) noexcept;

        bool OnlySendingEndpoints() const noexcept { return m_onlySendingEndpoints; }
        void OnlySendingEndpoints(_In_ bool value) noexcept;

        bool SourcesPaneOpen() const noexcept { return m_sourcesPaneOpen; }
        void SourcesPaneOpen(_In_ bool value) noexcept;

        // The Sources list's width as last dragged, in view pixels. 0 means the default.
        uint32_t SourcesPaneWidth() const noexcept { return m_sourcesPaneWidth; }
        void SourcesPaneWidth(_In_ uint32_t value) noexcept;

        static constexpr uint32_t MinimumRetainedMessageCount = 100;
        static constexpr uint32_t MaximumRetainedMessageCount = 500000;
        static constexpr uint32_t DefaultRetainedMessageCount = 10000;

        static constexpr uint32_t MinimumZoomPercent = 50;
        static constexpr uint32_t MaximumZoomPercent = 200;
        static constexpr uint32_t DefaultZoomPercent = 100;

        // font size the table uses at 100%
        static constexpr double BaseRowFontSize = 12.0;
        static constexpr double BaseChicletFontSize = 11.0;

        // comment gutter button at 100%; it only ever scales down, since the gutter is fixed width
        static constexpr double BaseCommentButtonSize = 22.0;
        static constexpr double BaseCommentGlyphSize = 12.0;

        static constexpr int32_t MinimumWindowWidth = 640;
        static constexpr int32_t MinimumWindowHeight = 400;

    private:
        AppSettings() noexcept;

        bool m_showClockMessages{ false };
        bool m_showActiveSenseMessages{ false };
        bool m_showMessageNameChiclets{ true };
        bool m_autoHideColumnsWhenNarrow{ true };
        TimestampDisplayFormat m_timestampFormat{ TimestampDisplayFormat::Ticks };
        uint32_t m_retainedMessageCount{ DefaultRetainedMessageCount };
        uint32_t m_tableZoomPercent{ DefaultZoomPercent };
        std::wstring m_columnLayout{};
        bool m_askBeforeClear{ true };
        std::wstring m_lastEndpointDeviceId{};
        uint8_t m_lastGroupNumber{ 0 };
        uint8_t m_lastChannelNumber{ 0 };
        bool m_advancedMode{ false };
        std::wstring m_advancedSources{};
        std::wstring m_advancedFilter{};
        bool m_foldRepeats{ false };
        bool m_showEndpointActivity{ true };
        bool m_onlySendingEndpoints{ false };
        bool m_sourcesPaneOpen{ true };
        uint32_t m_sourcesPaneWidth{ 0 };
    };
}
