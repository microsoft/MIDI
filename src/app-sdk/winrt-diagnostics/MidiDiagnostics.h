// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiDiagnostics.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::implementation
{
    struct MidiDiagnostics
    {
        MidiDiagnostics() = default;

        // these would be more robust if they did an enumeration lookup on the loopback/ping properties
        static winrt::hstring DiagnosticsLoopbackAEndpointDeviceId() noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A).c_str(); }
        static winrt::hstring DiagnosticsLoopbackBEndpointDeviceId() noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B).c_str(); }
        static winrt::hstring DiagnosticsInternalPingEndpointDeviceId() noexcept { return internal::NormalizeEndpointInterfaceIdHStringCopy(MIDI_DIAGNOSTICS_PING_BIDI_ID).c_str(); }


        static diag::MidiServicePingResponseSummary PingService(_In_ uint8_t pingCount) noexcept;
        static diag::MidiServicePingResponseSummary PingService(_In_ uint8_t pingCount, _In_ uint32_t timeoutMilliseconds) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::factory_implementation
{
    struct MidiDiagnostics : MidiDiagnosticsT<MidiDiagnostics, implementation::MidiDiagnostics>
    {
    };
}
