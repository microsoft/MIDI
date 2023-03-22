#include "pch.h"
#include "Enumeration.MidiDeviceManager.h"
#include "Enumeration.MidiDeviceManager.g.cpp"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    hstring MidiDeviceManager::CreateDevice(hstring const& providerId, hstring const& jsonProperties)
    {
        throw hresult_not_implemented();
    }
    bool MidiDeviceManager::DestroyDevice(hstring const& deviceId)
    {
        throw hresult_not_implemented();
    }
}
