// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINT_CUSTOMIZATION_PROCESSOR_H
#define MIDI_ENDPOINT_CUSTOMIZATION_PROCESSOR_H

#include <functional>
#include <memory>
#include <vector>

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointCustomPropertiesCache.h"
#include "MidiEndpointCustomizationProvenance.h"
#include "MidiEndpointMatchCriteria.h"
#include "MidiEndpointNameTable.h"

#define MIDI_CONFIG_JSON_TRANSPORT_COMMAND_LIST_ENDPOINT_CUSTOMIZATIONS                 L"listEndpointCustomizations"
#define MIDI_CONFIG_JSON_TRANSPORT_COMMAND_CAPABILITY_LIST_ENDPOINT_CUSTOMIZATIONS      MIDI_CONFIG_JSON_TRANSPORT_COMMAND_LIST_ENDPOINT_CUSTOMIZATIONS

#define MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATIONS_RESPONSE_ARRAY_KEY                     L"endpointCustomizations"
#define MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATION_RESOLVED_ENDPOINT_DEVICE_ID_KEY         L"resolvedEndpointDeviceId"
#define MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATION_HAS_USER_CONTENT_KEY                    L"hasUserContent"

namespace WindowsMidiServicesPluginConfigurationLib
{
    // What one update entry produced. The device properties are handed back rather than written,
    // because only the transport has the device manager. The name table has to travel with them:
    // the DEVPROPERTY entries point into it, so it must outlive the write.
    struct MidiEndpointCustomizationApplyResult
    {
        std::shared_ptr<MidiEndpointMatchCriteria> Match{ nullptr };
        std::shared_ptr<MidiEndpointCustomProperties> Properties{ nullptr };
        std::shared_ptr<MidiEndpointCustomizationProvenance> Provenance{ nullptr };
        std::shared_ptr<WindowsMidiServicesNamingLib::MidiEndpointNameTable> NameTable{ nullptr };

        winrt::hstring ResolvedEndpointDeviceId{};
        std::vector<DEVPROPERTY> EndpointProperties{};
    };

    // The update loop every transport that supports endpoint customization was carrying its own
    // copy of. The transport supplies only the two things that are genuinely its own: how to find
    // one of its instantiated endpoints, and what to do with the resulting device properties.
    class MidiEndpointCustomizationProcessor
    {
    public:
        using EndpointResolver = std::function<winrt::hstring(MidiEndpointMatchCriteria&)>;

        MidiEndpointCustomizationProcessor() = default;

        void Initialize(_In_ std::shared_ptr<MidiEndpointCustomPropertiesCache> cache) noexcept
        {
            m_cache = cache;
        }

        // Reads the "update" array, resolves and caches each entry, and builds its device
        // properties. Entries which resolve to nothing are still cached, which is how a
        // customization survives a device being unplugged.
        HRESULT ProcessUpdates(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& transportSection,
            _In_ EndpointResolver const& resolver,
            _In_ bool const rejectImagePaths,
            _Out_ std::vector<MidiEndpointCustomizationApplyResult>& results) noexcept;

        // Reads an "update" array under "remove" and forgets those customizations. Returns the
        // entries which had resolved to a live endpoint, with the properties needed to put that
        // endpoint back the way the transport supplied it.
        HRESULT ProcessRemovals(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& transportSection,
            _In_ EndpointResolver const& resolver,
            _Out_ std::vector<MidiEndpointCustomizationApplyResult>& results) noexcept;

        HRESULT WriteCustomizationsResponse(
            _In_ ::winrt::Windows::Data::Json::JsonObject& responseObject) noexcept;

    private:
        std::shared_ptr<MidiEndpointCustomPropertiesCache> m_cache{ nullptr };
    };
}

#endif
