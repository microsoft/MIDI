// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiEndpointDevicePropertyHelper.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDevicePropertyHelper
    {
        MidiEndpointDevicePropertyHelper() = default;

        static winrt::hstring GetMidiPropertyNameFromPropertyKey(_In_ winrt::guid const& fmtid, _In_ uint32_t const pid) noexcept;
        static winrt::hstring GetMidiPropertyNameFromPropertyKey(_In_ winrt::hstring const& key) noexcept;

        static collections::IMapView<winrt::hstring, winrt::hstring> GetAllMidiProperties() noexcept;

        static bool IsMidiPropertyKey(_In_ winrt::hstring const& key) noexcept;
        static bool IsMidiPropertyKey(_In_ winrt::guid fmtid, _In_ uint32_t const pid) noexcept;

    private:
        static bool m_initialized;
        static winrt::guid m_midiPropertyGuid;

        static collections::IMap<winrt::hstring, winrt::hstring> m_propertyFriendlyNames;    // string prop key, friendly name

        
        static winrt::hstring BuildPropertyStringKey(_In_ winrt::guid const& fmtid, _In_ uint32_t const pid);


        static void AddSingleMapEntry(_In_ winrt::hstring const& stringPropKey, _In_ winrt::hstring const& friendlyName);
        static void BuildPropertyMap();

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDevicePropertyHelper : MidiEndpointDevicePropertyHelperT<MidiEndpointDevicePropertyHelper, implementation::MidiEndpointDevicePropertyHelper, winrt::static_lifetime>
    {
    };
}
