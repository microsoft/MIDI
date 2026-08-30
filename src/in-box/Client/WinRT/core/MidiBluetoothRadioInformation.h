// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothRadioInformation.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothRadioInformation : MidiBluetoothRadioInformationT<MidiBluetoothRadioInformation>
    {
        MidiBluetoothRadioInformation() = default;

        bool IsPresent() const noexcept { return m_isPresent; }
        bool IsLowEnergySupported() const noexcept { return m_isLowEnergySupported; }
        bool IsCentralRoleSupported() const noexcept { return m_isCentralRoleSupported; }
        bool IsPeripheralRoleSupported() const noexcept { return m_isPeripheralRoleSupported; }

        void InternalInitializeFromJson(_In_ json::JsonObject const& radioJson) noexcept;

    private:
        bool m_isPresent{ false };
        bool m_isLowEnergySupported{ false };
        bool m_isCentralRoleSupported{ false };
        bool m_isPeripheralRoleSupported{ false };
    };
}
