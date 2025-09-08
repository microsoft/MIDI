// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


#ifndef VIRTUAL_PATCH_BAY_SHARED_TYPES_H
#define VIRTUAL_PATCH_BAY_SHARED_TYPES_H

#include <winrt/windows.foundation.h>

#undef GetObject
#include <winrt/windows.data.json.h>
namespace json = ::winrt::Windows::Data::Json;

#include "MidiEndpointMatchCriteria.h"


// virtual endpoints are used when we need to intercept incoming data
// from a client and then route it to other endpoints. The client connects
// to this endpoint
struct VirtualPatchBayEndpointDefinition
{
    winrt::guid ConfigIdentifier;

    winrt::hstring UniqueId{};
    winrt::hstring Name{};
    winrt::hstring Description{};
    winrt::hstring ImageFileName{};

    winrt::hstring CreatedEndpointDeviceId{};

    inline static VirtualPatchBayEndpointDefinition FromJson(_In_ json::JsonObject const& endpointEntry) noexcept
    {

    }

    inline json::JsonObject ToJson() noexcept
    {

    }
};



struct VirtualPatchBayRouteDefinition
{
    winrt::guid ConfigIdentifier;

    winrt::hstring UniqueId{};
    winrt::hstring Name{};
    winrt::hstring Description{};
    winrt::hstring ImageFileName{};

    winrt::hstring CreatedEndpointDeviceId{};

    inline static VirtualPatchBayEndpointDefinition FromJson(_In_ json::JsonObject const& endpointEntry) noexcept
    {

    }

    inline json::JsonObject ToJson() noexcept
    {

    }
};









#endif
