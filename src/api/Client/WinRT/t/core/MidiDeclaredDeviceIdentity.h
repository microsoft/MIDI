// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.MidiDeclaredDeviceIdentity.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeclaredDeviceIdentity : MidiDeclaredDeviceIdentityT<MidiDeclaredDeviceIdentity>
    {
        MidiDeclaredDeviceIdentity() = default;

        MidiDeclaredDeviceIdentity(
            uint8_t sysexIdByte1, uint8_t sysexIdByte2, uint8_t sysexIdByte3,
            uint8_t deviceFamilyLsb, uint8_t deviceFamilyMsb,
            uint8_t deviceFamilyModelNumberLsb, uint8_t deviceFamilyModelNumberMsb,
            uint8_t softwareRevisionLevelByte1, uint8_t softwareRevisionLevelByte2, uint8_t softwareRevisionLevelByte3, uint8_t softwareRevisionLevelByte4)
        {
            SetSystemExclusiveId(sysexIdByte1, sysexIdByte2, sysexIdByte3);
            SetDeviceFamily(deviceFamilyLsb, deviceFamilyMsb);
            SetDeviceFamilyModelNumber(deviceFamilyModelNumberLsb, deviceFamilyModelNumberMsb);
            SetSoftwareRevisionLevel(softwareRevisionLevelByte1, softwareRevisionLevelByte2, softwareRevisionLevelByte3, softwareRevisionLevelByte4);
        }

        bool IsReadOnly() const noexcept { return m_isReadOnly;}

        winrt::com_array<uint8_t> SystemExclusiveId() const noexcept { return winrt::com_array<uint8_t>(m_systemExclusiveIdBytes.begin(), m_systemExclusiveIdBytes.end()); }
        void SetSystemExclusiveId(_In_ uint8_t const byte1, _In_ uint8_t const byte2, _In_ uint8_t const byte3)
        {
            if (m_isReadOnly) return;

            m_systemExclusiveIdBytes = { byte1, byte2, byte3 };
        }
        
        uint8_t DeviceFamilyLsb() const noexcept { return m_deviceFamilyLsb; }
        uint8_t DeviceFamilyMsb() const noexcept { return m_deviceFamilyMsb; }
        void SetDeviceFamily(_In_ uint8_t const deviceFamilyLsb, _In_ uint8_t const deviceFamilyMsb)
        {
            if (m_isReadOnly) return;

            m_deviceFamilyLsb = deviceFamilyLsb;
            m_deviceFamilyMsb = deviceFamilyMsb;
        }
        
        uint8_t DeviceFamilyModelNumberLsb() const noexcept { return m_deviceFamilyModelNumberLsb; }
        uint8_t DeviceFamilyModelNumberMsb() const noexcept { return m_deviceFamilyModelNumberMsb; }
        void SetDeviceFamilyModelNumber(_In_ uint8_t const deviceFamilyModelNumberLsb, _In_ uint8_t const deviceFamilyModelNumberMsb)
        {
            if (m_isReadOnly) return;

            m_deviceFamilyModelNumberLsb = deviceFamilyModelNumberLsb;
            m_deviceFamilyModelNumberMsb = deviceFamilyModelNumberMsb;
        }
        
        winrt::com_array<uint8_t> SoftwareRevisionLevel() const noexcept { return winrt::com_array<uint8_t>(m_softwareRevisionLevelBytes.begin(), m_softwareRevisionLevelBytes.end()); }
        void SetSoftwareRevisionLevel(_In_ uint8_t const byte1, _In_ uint8_t const byte2, _In_ uint8_t const byte3, _In_ uint8_t const byte4)
        {
            if (m_isReadOnly) return;

            m_softwareRevisionLevelBytes = { byte1, byte2, byte3, byte4 };
        }

        void InternalSetReadOnly() { m_isReadOnly = true; }

    private:
        bool m_isReadOnly{ false };
        uint8_t m_deviceFamilyLsb{ 0 };
        uint8_t m_deviceFamilyMsb{ 0 };
        uint8_t m_deviceFamilyModelNumberLsb{ 0 };
        uint8_t m_deviceFamilyModelNumberMsb{ 0 };
        winrt::com_array<uint8_t> m_systemExclusiveIdBytes { winrt::com_array<uint8_t>({ (uint8_t)0,(uint8_t)0,(uint8_t)0 }) };
        winrt::com_array<uint8_t> m_softwareRevisionLevelBytes { winrt::com_array<uint8_t>({ (uint8_t)0,(uint8_t)0,(uint8_t)0,(uint8_t)0 }) };

    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeclaredDeviceIdentity : MidiDeclaredDeviceIdentityT<MidiDeclaredDeviceIdentity, implementation::MidiDeclaredDeviceIdentity>
    {};
}
