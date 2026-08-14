// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Reporting.MidiReporting.g.h"

namespace winrt::Windows::Devices::Midi2::Reporting::implementation
{
    struct MidiReporting
    {
        //MidiReporting() = default;

        static collections::IVectorView<rpt::MidiServiceTransportPluginInfo> GetInstalledTransportPlugins() noexcept;
        //static collections::IVector<rept::MidiServiceMessageProcessingPluginInfo> GetInstalledMessageProcessingPlugins();
        static collections::IVectorView<rpt::MidiServiceSessionInfo> GetActiveSessions() noexcept;

        static collections::IVectorView<rpt::MidiServiceSessionInfo> FindAllSessionsWithMatchingOpenUmpEndpoint(_In_ winrt::hstring const& endpointDeviceId, bool const includeRelatedMidi1Ports) noexcept;
        static collections::IVectorView<rpt::MidiServiceSessionInfo> FindAllSessionsWithMatchingOpenUmpEndpointOrMidi1Ports(_In_ collections::IVectorView<winrt::hstring> const& endpointsAndPorts) noexcept;

    private:
        static collections::IVectorView<rpt::MidiServiceSessionInfo> GetActiveSessionsInternal(_In_ collections::IVectorView<winrt::hstring> const& filterEndpointAndPortIds);


    };
}
namespace winrt::Windows::Devices::Midi2::Reporting::factory_implementation
{
    struct MidiReporting : MidiReportingT<MidiReporting, implementation::MidiReporting, winrt::static_lifetime>
    {
    };
}

