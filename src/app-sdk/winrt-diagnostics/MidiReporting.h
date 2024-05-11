#pragma once
#include "MidiReporting.g.h"

namespace winrt::Microsoft::Devices::Midi2::Diagnostics::implementation
{
    struct MidiReporting
    {
        MidiReporting() = default;

        static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::Diagnostics::MidiServiceTransportPluginInfo> GetInstalledTransportPlugins();
        static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::Diagnostics::MidiServiceMessageProcessingPluginInfo> GetInstalledMessageProcessingPlugins();
        static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::Diagnostics::MidiServiceSessionInfo> GetActiveSessions();
    };
}
namespace winrt::Microsoft::Devices::Midi2::Diagnostics::factory_implementation
{
    struct MidiReporting : MidiReportingT<MidiReporting, implementation::MidiReporting>
    {
    };
}
