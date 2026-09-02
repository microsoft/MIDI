// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midikeyboard
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midikeyboard)";

        constexpr wchar_t ValueConnection[] = L"Connection";
        constexpr wchar_t ValueEndpointDeviceId[] = L"EndpointDeviceId";
        constexpr wchar_t ValueTransmitGroup[] = L"TransmitGroup";
        constexpr wchar_t ValueTransmitChannel[] = L"TransmitChannel";
        constexpr wchar_t ValueBaseOctave[] = L"BaseOctave";
        constexpr wchar_t ValueOctaveCount[] = L"OctaveCount";
        constexpr wchar_t ValueTranspose[] = L"Transpose";
        constexpr wchar_t ValueRibbons[] = L"Ribbons";
        constexpr wchar_t ValueKeyPressure[] = L"KeyPressure";
        constexpr wchar_t ValuePerNoteController[] = L"PerNoteController";
        constexpr wchar_t ValueVelocityMode[] = L"VelocityMode";
        constexpr wchar_t ValueVelocityMinimum[] = L"VelocityMinimum";
        constexpr wchar_t ValueVelocityMaximum[] = L"VelocityMaximum";
        constexpr wchar_t ValueFixedVelocity[] = L"FixedVelocity";
        constexpr wchar_t ValueShowComputerKeys[] = L"ShowComputerKeys";
        constexpr wchar_t ValueShowNoteNames[] = L"ShowNoteNames";
        constexpr wchar_t ValueArpeggiator[] = L"Arpeggiator";
        constexpr wchar_t ValueArpeggiatorBpm[] = L"ArpeggiatorBpm";
        constexpr wchar_t ValueArpeggiatorRate[] = L"ArpeggiatorRate";

        template <typename TEnum>
        TEnum ReadEnum(uint32_t stored, TEnum maximum, TEnum fallback) noexcept
        {
            return stored <= static_cast<uint32_t>(maximum)
                ? static_cast<TEnum>(stored)
                : fallback;
        }
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        m_connection = ReadEnum(
            ReadDword(ValueConnection, static_cast<uint32_t>(ConnectionMode::VirtualDevice)),
            ConnectionMode::ExistingEndpoint, ConnectionMode::VirtualDevice);

        m_endpointDeviceId = ReadString(ValueEndpointDeviceId, L"");

        m_transmitGroupNumber = std::clamp(ReadDword(ValueTransmitGroup, 1u), 1u, 16u);
        m_transmitChannelNumber = std::clamp(ReadDword(ValueTransmitChannel, 1u), 1u, 16u);

        m_baseOctave = std::clamp(
            static_cast<int32_t>(ReadDword(ValueBaseOctave, static_cast<uint32_t>(1))),
            MinimumBaseOctave, MaximumBaseOctave);

        m_octaveCount = std::clamp(ReadDword(ValueOctaveCount, 4u), MinimumOctaveCount, MaximumOctaveCount);

        m_transpose = std::clamp(
            static_cast<int32_t>(ReadDword(ValueTranspose, static_cast<uint32_t>(0))),
            MinimumTranspose, MaximumTranspose);

        m_ribbons = ReadEnum(
            ReadDword(ValueRibbons, static_cast<uint32_t>(RibbonPosition::Left)),
            RibbonPosition::Disabled, RibbonPosition::Left);

        m_keyPressure = ReadEnum(
            ReadDword(ValueKeyPressure, static_cast<uint32_t>(KeyPressureMode::PolyPressure)),
            KeyPressureMode::PolyPressure, KeyPressureMode::PolyPressure);

        m_perNoteControllerIndex = std::clamp(ReadDword(ValuePerNoteController, 1u), 0u, 127u);

        m_velocity = ReadEnum(
            ReadDword(ValueVelocityMode, static_cast<uint32_t>(VelocityMode::KeyLocation)),
            VelocityMode::Off, VelocityMode::KeyLocation);

        m_velocityMinimum = std::clamp(ReadDword(ValueVelocityMinimum, 30u), MinimumVelocity, MaximumVelocity);
        m_velocityMaximum = std::clamp(ReadDword(ValueVelocityMaximum, 127u), MinimumVelocity, MaximumVelocity);
        m_fixedVelocity = std::clamp(ReadDword(ValueFixedVelocity, 100u), MinimumVelocity, MaximumVelocity);

        if (m_velocityMaximum < m_velocityMinimum)
        {
            std::swap(m_velocityMinimum, m_velocityMaximum);
        }

        m_showComputerKeys = ReadDword(ValueShowComputerKeys, 1u) != 0;
        m_showNoteNames = ReadDword(ValueShowNoteNames, 1u) != 0;

        m_arpeggiator = ReadEnum(
            ReadDword(ValueArpeggiator, static_cast<uint32_t>(ArpeggiatorMode::Off)),
            ArpeggiatorMode::AsPlayed, ArpeggiatorMode::Off);

        m_arpeggiatorBpm = std::clamp(ReadDword(ValueArpeggiatorBpm, 120u), MinimumBpm, MaximumBpm);

        m_arpeggiatorRate = ReadEnum(
            ReadDword(ValueArpeggiatorRate, static_cast<uint32_t>(ArpeggiatorDivision::Sixteenth)),
            ArpeggiatorDivision::ThirtySecond, ArpeggiatorDivision::Sixteenth);
    }

    void AppSettings::Connection(ConnectionMode value) noexcept
    {
        m_connection = value;
        WriteDword(ValueConnection, static_cast<uint32_t>(value));
    }

    void AppSettings::EndpointDeviceId(std::wstring const& value) noexcept
    {
        m_endpointDeviceId = value;
        WriteString(ValueEndpointDeviceId, value);
    }

    void AppSettings::TransmitGroupNumber(uint32_t value) noexcept
    {
        m_transmitGroupNumber = std::clamp(value, 1u, 16u);
        WriteDword(ValueTransmitGroup, m_transmitGroupNumber);
    }

    void AppSettings::TransmitChannelNumber(uint32_t value) noexcept
    {
        m_transmitChannelNumber = std::clamp(value, 1u, 16u);
        WriteDword(ValueTransmitChannel, m_transmitChannelNumber);
    }

    void AppSettings::BaseOctave(int32_t value) noexcept
    {
        m_baseOctave = std::clamp(value, MinimumBaseOctave, MaximumBaseOctave);
        WriteDword(ValueBaseOctave, static_cast<uint32_t>(m_baseOctave));
    }

    void AppSettings::OctaveCount(uint32_t value) noexcept
    {
        m_octaveCount = std::clamp(value, MinimumOctaveCount, MaximumOctaveCount);
        WriteDword(ValueOctaveCount, m_octaveCount);
    }

    void AppSettings::Transpose(int32_t value) noexcept
    {
        m_transpose = std::clamp(value, MinimumTranspose, MaximumTranspose);
        WriteDword(ValueTranspose, static_cast<uint32_t>(m_transpose));
    }

    void AppSettings::Ribbons(RibbonPosition value) noexcept
    {
        m_ribbons = value;
        WriteDword(ValueRibbons, static_cast<uint32_t>(value));
    }

    void AppSettings::KeyPressure(KeyPressureMode value) noexcept
    {
        m_keyPressure = value;
        WriteDword(ValueKeyPressure, static_cast<uint32_t>(value));
    }

    void AppSettings::PerNoteControllerIndex(uint32_t value) noexcept
    {
        m_perNoteControllerIndex = std::clamp(value, 0u, 127u);
        WriteDword(ValuePerNoteController, m_perNoteControllerIndex);
    }

    void AppSettings::Velocity(VelocityMode value) noexcept
    {
        m_velocity = value;
        WriteDword(ValueVelocityMode, static_cast<uint32_t>(value));
    }

    void AppSettings::VelocityMinimum(uint32_t value) noexcept
    {
        m_velocityMinimum = std::clamp(value, MinimumVelocity, MaximumVelocity);
        WriteDword(ValueVelocityMinimum, m_velocityMinimum);
    }

    void AppSettings::VelocityMaximum(uint32_t value) noexcept
    {
        m_velocityMaximum = std::clamp(value, MinimumVelocity, MaximumVelocity);
        WriteDword(ValueVelocityMaximum, m_velocityMaximum);
    }

    void AppSettings::FixedVelocity(uint32_t value) noexcept
    {
        m_fixedVelocity = std::clamp(value, MinimumVelocity, MaximumVelocity);
        WriteDword(ValueFixedVelocity, m_fixedVelocity);
    }

    void AppSettings::ShowComputerKeys(bool value) noexcept
    {
        m_showComputerKeys = value;
        WriteDword(ValueShowComputerKeys, value ? 1u : 0u);
    }

    void AppSettings::ShowNoteNames(bool value) noexcept
    {
        m_showNoteNames = value;
        WriteDword(ValueShowNoteNames, value ? 1u : 0u);
    }

    void AppSettings::Arpeggiator(ArpeggiatorMode value) noexcept
    {
        m_arpeggiator = value;
        WriteDword(ValueArpeggiator, static_cast<uint32_t>(value));
    }

    void AppSettings::ArpeggiatorBpm(uint32_t value) noexcept
    {
        m_arpeggiatorBpm = std::clamp(value, MinimumBpm, MaximumBpm);
        WriteDword(ValueArpeggiatorBpm, m_arpeggiatorBpm);
    }

    void AppSettings::ArpeggiatorRate(ArpeggiatorDivision value) noexcept
    {
        m_arpeggiatorRate = value;
        WriteDword(ValueArpeggiatorRate, static_cast<uint32_t>(value));
    }
}
