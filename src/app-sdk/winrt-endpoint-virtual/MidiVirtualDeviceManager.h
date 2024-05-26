#pragma once
#include "MidiVirtualDeviceManager.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    struct MidiVirtualDeviceManager : MidiVirtualDeviceManagerT<MidiVirtualDeviceManager>
    {
        MidiVirtualDeviceManager() = default;

        static bool IsTransportAvailable() noexcept;
        static const winrt::guid AbstractionId() noexcept { return internal::StringToGuid(L"{8FEAAD91-70E1-4A19-997A-377720A719C1}"); }

        static virt::MidiVirtualDeviceCreationResult CreateVirtualDevice(_In_ virt::MidiVirtualDeviceCreationConfig creationConfig) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::factory_implementation
{
    struct MidiVirtualDeviceManager : MidiVirtualDeviceManagerT<MidiVirtualDeviceManager, implementation::MidiVirtualDeviceManager, winrt::static_lifetime>
    {
    };
}
