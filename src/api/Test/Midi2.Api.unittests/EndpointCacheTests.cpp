// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

using namespace winrt::Windows::Devices::Midi2;

TEST_CASE("Offline.Cache.Endpoint Basics")
{
    SECTION("Add and Read Item")
    {
        std::cout << "Add and Read Endpoint Cache Item" << std::endl;

        winrt::hstring propertyKey = L"TEST_KEY";
        winrt::hstring endpointDeviceId = LOOPBACK_BIDI_ID_A;
        winrt::hstring data = L"{ Some Test Data }";


        REQUIRE_NOTHROW(MidiService::EndpointMetadataCache());


        // add item to the cache
        MidiService::EndpointMetadataCache().AddOrUpdateData(endpointDeviceId, propertyKey, data);

        REQUIRE(MidiService::EndpointMetadataCache().IsDataPresent(endpointDeviceId, propertyKey));

        // get the item back from the cache

        auto retrievedData = MidiService::EndpointMetadataCache().GetData(endpointDeviceId, propertyKey);

        REQUIRE(retrievedData == data);

        std::cout << " - Done" << std::endl;

    }
}
