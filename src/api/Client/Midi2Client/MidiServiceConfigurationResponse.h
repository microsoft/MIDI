// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServiceConfigurationResponse.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiServiceConfigurationResponse : MidiServiceConfigurationResponseT<MidiServiceConfigurationResponse>
    {
        MidiServiceConfigurationResponse() = default;

        midi2::MidiServiceConfigurationResponseStatus Status() noexcept { return m_status; }

        winrt::hstring ResponseJson() { return m_responseJson; }

        void InternalSetStatus(_In_ midi2::MidiServiceConfigurationResponseStatus const status) noexcept { m_status = status; }
        void InternalSetResponseJson(_In_ winrt::hstring const& responseJson) noexcept { m_responseJson = responseJson; }

    private:
        midi2::MidiServiceConfigurationResponseStatus m_status{ midi2::MidiServiceConfigurationResponseStatus::ErrorOther };

        winrt::hstring m_responseJson{};
    };
}
