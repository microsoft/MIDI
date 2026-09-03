// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midikeyboard
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class ConnectionMode : int32_t
    {
        // this app is the device other apps connect to. No group or channel choice: the
        // single bidirectional function block owns group 1.
        VirtualDevice = 0,

        // play an endpoint that already exists, such as a loopback or an instrument
        ExistingEndpoint = 1
    };

    enum class RibbonPosition : int32_t
    {
        Left = 0,
        Right = 1,
        Disabled = 2
    };

    enum class KeyPressureMode : int32_t
    {
        Off = 0,

        // MIDI 2.0 registered per-note controller, controller number below
        PerNoteController = 1,

        // one value for the whole channel
        ChannelPressure = 2,

        // one value per sounding note
        PolyPressure = 3
    };

    enum class VelocityMode : int32_t
    {
        // where on the key it was struck: the closer to the player, the harder
        KeyLocation = 0,

        // always the configured fixed velocity
        Fixed = 1,

        // no velocity sensitivity at all, every note at full scale
        Off = 2
    };

    enum class ArpeggiatorMode : int32_t
    {
        Off = 0,
        Up = 1,
        UpDown = 2,

        // up and down with the top and bottom notes played twice
        UpDownRepeat = 3,

        Down = 4,
        Random = 5,

        // the order the keys were pressed in
        AsPlayed = 6
    };

    enum class ArpeggiatorDivision : int32_t
    {
        Quarter = 0,
        Eighth = 1,
        EighthTriplet = 2,
        Sixteenth = 3,
        SixteenthTriplet = 4,
        ThirtySecond = 5
    };

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        ConnectionMode Connection() const noexcept { return m_connection; }
        void Connection(ConnectionMode value) noexcept;

        std::wstring const& EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        void EndpointDeviceId(std::wstring const& value) noexcept;

        // user-facing numbers (1-16), not indexes
        uint32_t TransmitGroupNumber() const noexcept { return m_transmitGroupNumber; }
        void TransmitGroupNumber(uint32_t value) noexcept;

        uint32_t TransmitChannelNumber() const noexcept { return m_transmitChannelNumber; }
        void TransmitChannelNumber(uint32_t value) noexcept;

        // octave of the leftmost C, in the numbering where note 60 is C3
        int32_t BaseOctave() const noexcept { return m_baseOctave; }
        void BaseOctave(int32_t value) noexcept;

        uint32_t OctaveCount() const noexcept { return m_octaveCount; }
        void OctaveCount(uint32_t value) noexcept;

        int32_t Transpose() const noexcept { return m_transpose; }
        void Transpose(int32_t value) noexcept;

        RibbonPosition Ribbons() const noexcept { return m_ribbons; }
        void Ribbons(RibbonPosition value) noexcept;

        KeyPressureMode KeyPressure() const noexcept { return m_keyPressure; }
        void KeyPressure(KeyPressureMode value) noexcept;

        uint32_t PerNoteControllerIndex() const noexcept { return m_perNoteControllerIndex; }
        void PerNoteControllerIndex(uint32_t value) noexcept;

        VelocityMode Velocity() const noexcept { return m_velocity; }
        void Velocity(VelocityMode value) noexcept;

        uint32_t VelocityMinimum() const noexcept { return m_velocityMinimum; }
        void VelocityMinimum(uint32_t value) noexcept;

        uint32_t VelocityMaximum() const noexcept { return m_velocityMaximum; }
        void VelocityMaximum(uint32_t value) noexcept;

        uint32_t FixedVelocity() const noexcept { return m_fixedVelocity; }
        void FixedVelocity(uint32_t value) noexcept;

        bool ShowComputerKeys() const noexcept { return m_showComputerKeys; }
        void ShowComputerKeys(bool value) noexcept;

        bool ShowNoteNames() const noexcept { return m_showNoteNames; }
        void ShowNoteNames(bool value) noexcept;

        ArpeggiatorMode Arpeggiator() const noexcept { return m_arpeggiator; }
        void Arpeggiator(ArpeggiatorMode value) noexcept;

        uint32_t ArpeggiatorBpm() const noexcept { return m_arpeggiatorBpm; }
        void ArpeggiatorBpm(uint32_t value) noexcept;

        ArpeggiatorDivision ArpeggiatorRate() const noexcept { return m_arpeggiatorRate; }
        void ArpeggiatorRate(ArpeggiatorDivision value) noexcept;

        static constexpr int32_t MinimumBaseOctave = -2;
        static constexpr int32_t MaximumBaseOctave = 8;
        static constexpr uint32_t MinimumOctaveCount = 1;
        static constexpr uint32_t MaximumOctaveCount = 10;
        static constexpr int32_t MinimumTranspose = -24;
        static constexpr int32_t MaximumTranspose = 24;
        static constexpr uint32_t MinimumBpm = 20;
        static constexpr uint32_t MaximumBpm = 400;

        // MIDI 1.0 style velocity numbers, which is how players think about them. They are
        // scaled up to the 16 bit MIDI 2.0 range on the way out.
        static constexpr uint32_t MinimumVelocity = 1;
        static constexpr uint32_t MaximumVelocity = 127;

    private:
        AppSettings() noexcept;

        ConnectionMode m_connection{ ConnectionMode::VirtualDevice };
        std::wstring m_endpointDeviceId{};
        uint32_t m_transmitGroupNumber{ 1 };
        uint32_t m_transmitChannelNumber{ 1 };

        int32_t m_baseOctave{ 1 };

        // three octaves is the most that still leaves the keys playable at the default window
        // size; a wider window is what earns more of them
        uint32_t m_octaveCount{ 3 };

        int32_t m_transpose{ 0 };

        RibbonPosition m_ribbons{ RibbonPosition::Left };

        KeyPressureMode m_keyPressure{ KeyPressureMode::PolyPressure };
        uint32_t m_perNoteControllerIndex{ 1 };

        VelocityMode m_velocity{ VelocityMode::KeyLocation };
        uint32_t m_velocityMinimum{ 30 };
        uint32_t m_velocityMaximum{ 127 };
        uint32_t m_fixedVelocity{ 100 };

        bool m_showComputerKeys{ true };
        bool m_showNoteNames{ true };

        ArpeggiatorMode m_arpeggiator{ ArpeggiatorMode::Off };
        uint32_t m_arpeggiatorBpm{ 120 };
        ArpeggiatorDivision m_arpeggiatorRate{ ArpeggiatorDivision::Sixteenth };
    };
}
