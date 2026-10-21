// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

#include <string>
#include <vector>

namespace midisequencer
{
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    // A device that gets this sequencer's clock. Kept by id and by name, like the metronome.
    // Studio setup chooses these, one for each device and group.
    struct ClockOutputSetting
    {
        std::wstring EndpointId{};
        std::wstring EndpointName{};
        uint8_t Group{ 0 };
        bool Clock{ true };
        bool StartStop{ true };
        bool SongPosition{ true };

        // A MIDI 2.0 Set Tempo message at each change. Only sent to a group that speaks MIDI 2.0.
        bool Tempo{ true };

        // Positive sends later, negative sends earlier.
        int32_t OffsetMilliseconds{ 0 };
    };

    // A device's group the sequencer leaves alone one way or both, chosen in Studio setup. Kept
    // by id and by name, like the clock outputs.
    struct PortSetting
    {
        std::wstring EndpointId{};
        std::wstring EndpointName{};
        uint8_t Group{ 0 };
        bool PlayTo{ true };
        bool RecordFrom{ true };
    };

    // Where MIDI Time Code goes, if anywhere.
    struct TimeCodeSetting
    {
        // Off keeps the device, so turning it on again needs no new choice.
        bool Enabled{ false };

        std::wstring EndpointId{};
        std::wstring EndpointName{};
        uint8_t Group{ 0 };

        // midiapp::MidiTimeCodeFrameRate: 0 is 24, 1 is 25, 2 is 29.97 drop and 3 is 30.
        int32_t Rate{ 3 };

        // The time code at the top of the sequence.
        std::wstring Start{ L"01:00:00:00" };

        bool IsOn() const noexcept { return Enabled && (!EndpointId.empty() || !EndpointName.empty()); }
    };

    // The device whose MIDI clock the sequencer follows, when it doesn't keep its own tempo.
    struct FollowClockSetting
    {
        // Off keeps the device, so following it again needs no new choice.
        bool Enabled{ false };

        std::wstring EndpointId{};
        std::wstring EndpointName{};
        uint8_t Group{ 0 };

        bool IsOn() const noexcept { return Enabled && (!EndpointId.empty() || !EndpointName.empty()); }
    };

    // How values are shown in the editors. The file always holds MIDI 2.0 resolution.
    enum class ValueDisplay : uint32_t
    {
        Midi2 = 0,      // 16-bit velocity, 32-bit controllers
        Midi1 = 1,      // 0 to 127
        Percent = 2,
    };

    // Settings that belong to this PC rather than to a sequence, because they name a device or
    // describe how this person likes to work.
    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // The metronome. The endpoint is kept by id and by name, so it can be found again after
        // the device is plugged into another port.
        bool MetronomeEnabled() const noexcept { return m_metronomeEnabled; }
        void MetronomeEnabled(bool value) noexcept;

        bool MetronomeOnlyWhileRecording() const noexcept { return m_metronomeOnlyWhileRecording; }
        void MetronomeOnlyWhileRecording(bool value) noexcept;

        std::wstring const& MetronomeEndpointId() const noexcept { return m_metronomeEndpointId; }
        std::wstring const& MetronomeEndpointName() const noexcept { return m_metronomeEndpointName; }
        void MetronomeEndpoint(_In_ std::wstring const& id, _In_ std::wstring const& name) noexcept;

        uint8_t MetronomeGroup() const noexcept { return m_metronomeGroup; }
        void MetronomeGroup(uint8_t value) noexcept;

        uint8_t MetronomeChannel() const noexcept { return m_metronomeChannel; }
        void MetronomeChannel(uint8_t value) noexcept;

        uint8_t MetronomeNote() const noexcept { return m_metronomeNote; }
        void MetronomeNote(uint8_t value) noexcept;

        bool ShowLauncher() const noexcept { return m_showLauncher; }
        void ShowLauncher(bool value) noexcept;

        bool ShowMixer() const noexcept { return m_showMixer; }
        void ShowMixer(bool value) noexcept;

        // The timeline's scroll bar: an overview of the sequence or a plain bar, under the
        // timeline or above the ruler (design section 31).
        bool TimelineOverview() const noexcept { return m_timelineOverview; }
        void TimelineOverview(bool value) noexcept;
        bool TimelineBarAtTop() const noexcept { return m_timelineBarAtTop; }
        void TimelineBarAtTop(bool value) noexcept;

        // A sequence with a file saves itself a moment after each change, and when the window
        // closes. Off, it's saved only when asked, and closing asks about unsaved changes.
        bool AutoSave() const noexcept { return m_autoSave; }
        void AutoSave(bool value) noexcept;

        // Pixels for one bar on the timeline, at 100% scaling.
        double BarWidth() const noexcept { return m_barWidth; }
        void BarWidth(double value) noexcept;

        // Snap and launch quantization, in ticks. 0 is off for snap and immediately for launch.
        uint32_t SnapTicks() const noexcept { return m_snapTicks; }
        void SnapTicks(uint32_t value) noexcept;

        uint32_t LaunchQuantizeTicks() const noexcept { return m_launchQuantizeTicks; }
        void LaunchQuantizeTicks(uint32_t value) noexcept;

        // How many bars a take recorded into a launcher slot lasts: 1, 2, 4, 8 or 16.
        uint32_t SlotRecordBars() const noexcept { return m_slotRecordBars; }
        void SlotRecordBars(uint32_t value) noexcept;

        double EditorHeight() const noexcept { return m_editorHeight; }
        void EditorHeight(double value) noexcept;

        // How wide the track list and the clip launcher are, in pixels at 100% scaling.
        double HeaderWidth() const noexcept { return m_headerWidth; }
        void HeaderWidth(double value) noexcept;

        double LauncherWidth() const noexcept { return m_launcherWidth; }
        void LauncherWidth(double value) noexcept;

        static constexpr double DefaultHeaderWidth = 268.0;
        static constexpr double MinimumHeaderWidth = 200.0;
        static constexpr double MaximumHeaderWidth = 640.0;
        static constexpr double DefaultLauncherWidth = 500.0;
        static constexpr double MinimumLauncherWidth = 120.0;
        static constexpr double MaximumLauncherWidth = 4000.0;

        ValueDisplay ValuesAs() const noexcept { return m_valuesAs; }
        void ValuesAs(ValueDisplay value) noexcept;

        std::wstring const& LastFolder() const noexcept { return m_lastFolder; }
        void LastFolder(_In_ std::wstring const& value) noexcept;

        // Whether the sequencer sends early by each device's offset from MIDI Settings. On by
        // default because the service in Windows doesn't; a service that does needs it off.
        bool ApplyDeviceOffsets() const noexcept { return m_applyDeviceOffsets; }
        void ApplyDeviceOffsets(bool value) noexcept;

        // Whether turning a pattern step on plays its note, so a drum can be heard as it's added.
        bool HearPatternSteps() const noexcept { return m_hearPatternSteps; }
        void HearPatternSteps(bool value) noexcept;

        // The devices that follow this sequencer's clock, and where time code goes.
        std::vector<ClockOutputSetting> const& ClockOutputs() const noexcept { return m_clockOutputs; }
        void ClockOutputs(_In_ std::vector<ClockOutputSetting> const& value) noexcept;

        // Device groups the sequencer doesn't play to, or doesn't record from, chosen in Studio
        // setup. Only groups with something turned off are kept.
        std::vector<PortSetting> const& PortsOff() const noexcept { return m_portsOff; }
        void PortsOff(_In_ std::vector<PortSetting> const& value) noexcept;

        // The recent sequences and the pinned ones, as the list's own text. Read again each time,
        // because every sequencer window on the PC writes it.
        std::wstring RecentSequencesText() const noexcept;
        void RecentSequencesText(_In_ std::wstring const& value) const noexcept;

        TimeCodeSetting const& TimeCode() const noexcept { return m_timeCode; }
        void TimeCode(_In_ TimeCodeSetting const& value) noexcept;

        FollowClockSetting const& FollowClock() const noexcept { return m_followClock; }
        void FollowClock(_In_ FollowClockSetting const& value) noexcept;

        // Whether the sequencer makes its own MIDI endpoint for remote control. Off by default,
        // so a PC's device lists don't get an endpoint nobody asked for.
        bool RemoteControlEnabled() const noexcept { return m_remoteControlEnabled; }
        void RemoteControlEnabled(bool value) noexcept;

        // The look picked in Appearance: a theme's id ("night-drive", "tactile"). An id that isn't
        // a theme any more falls back to Night drive when the window applies it.
        std::wstring const& StyleId() const noexcept { return m_styleId; }
        void StyleId(_In_ std::wstring const& value) noexcept;

        static constexpr wchar_t DefaultStyleId[] = L"night-drive";

        static constexpr size_t MaximumClockOutputs = 16;
        static constexpr int32_t MaximumClockOffsetMilliseconds = 1000;
        static constexpr size_t MaximumPortsOff = 256;

        static constexpr double DefaultBarWidth = 34.0;
        static constexpr double MinimumBarWidth = 6.0;
        static constexpr double MaximumBarWidth = 2400.0;

    private:
        AppSettings() noexcept;

        bool m_metronomeEnabled{ false };
        bool m_metronomeOnlyWhileRecording{ false };
        std::wstring m_metronomeEndpointId{};
        std::wstring m_metronomeEndpointName{};
        uint8_t m_metronomeGroup{ 0 };
        uint8_t m_metronomeChannel{ 9 };
        uint8_t m_metronomeNote{ 37 };

        bool m_showLauncher{ true };
        bool m_showMixer{ false };
        bool m_timelineOverview{ true };
        bool m_timelineBarAtTop{ false };
        bool m_autoSave{ true };
        double m_barWidth{ DefaultBarWidth };
        uint32_t m_snapTicks{ 240 };
        uint32_t m_launchQuantizeTicks{ 3840 };
        uint32_t m_slotRecordBars{ 4 };
        double m_editorHeight{ 300 };
        double m_headerWidth{ DefaultHeaderWidth };
        double m_launcherWidth{ DefaultLauncherWidth };
        ValueDisplay m_valuesAs{ ValueDisplay::Midi2 };
        std::wstring m_lastFolder{};
        bool m_applyDeviceOffsets{ true };
        bool m_hearPatternSteps{ true };
        std::vector<ClockOutputSetting> m_clockOutputs{};
        std::vector<PortSetting> m_portsOff{};
        TimeCodeSetting m_timeCode{};
        FollowClockSetting m_followClock{};
        bool m_remoteControlEnabled{ false };
        std::wstring m_styleId{ DefaultStyleId };
    };
}
