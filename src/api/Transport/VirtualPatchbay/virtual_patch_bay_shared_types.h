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
struct InternalVirtualPatchBayEndpointDefinition
{
    winrt::guid RouteId;
     
    winrt::hstring UniqueId{};
    winrt::hstring Name{};
    winrt::hstring Description{};
    winrt::hstring ImageFileName{};

    winrt::hstring CreatedEndpointDeviceId{};

    inline static InternalVirtualPatchBayEndpointDefinition FromJson(_In_ json::JsonObject const& endpointEntry) noexcept
    {

    }

    // JSON format
    //
    // "{transport_id...}":
    // {
    // 
    // }
    //


    inline json::JsonObject GetConfigJson() noexcept
    {

    }
};


struct InternalVirtualPatchBaySourceDefinition
{
    bool IsEnabled{ false };

    // TODO: Match Criteria

    std::vector<byte> IncludedMessageTypes{ };
    std::vector<byte> IncludedGroupIndexes{ };
    std::vector<byte> IncludedChannelIndexes{ };

    std::vector<byte> IncludedMidi2ChannelVoiceMessageStatuses{ };
    std::vector<byte> IncludedMidi1ChannelVoiceMessageStatuses{ };

};

// TODO: Should these just use the service IMidiTransform 
//       to be open to most any kind of transform? Maybe we
//       switch to that once we have dynamic/configurable
//       transforms set up.


struct InternalMidiTransformConfiguration
{
    // the id which uniquely identifies this instance of the transform
    winrt::guid TransformInstanceId{};

    // id of the transform (clsid if we move this to using COM)
    winrt::guid TransformId{};

    // metadata for the transform
    winrt::hstring Name{};
    winrt::hstring Description{};

    virtual json::JsonObject GetConfigJson() noexcept = 0;
};

struct InternalMidiGroupTransformConfiguration : InternalMidiTransformConfiguration
{
    // if an entry exists in this table, the source group index is changed to the destination
    // group index upon processing. If the entry does not exist, then no change is made.
    std::map<byte, byte> GroupMap{};

    inline json::JsonObject GetConfigJson() noexcept
    {
    }
};

struct InternalMidiChannelTransformConfiguration : InternalMidiTransformConfiguration
{

    inline json::JsonObject GetConfigJson() noexcept
    {
    }
};

struct InternalMidiMessageTypeTransformConfiguration : InternalMidiTransformConfiguration
{

    inline json::JsonObject GetConfigJson() noexcept
    {
    }
};

struct InternalMidi1ChannelVoiceStatusMessageTransformConfiguration : InternalMidiTransformConfiguration
{

    inline json::JsonObject GetConfigJson() noexcept
    {
    }
};

struct InternalMidi2ChannelVoiceStatusMessageTransformConfiguration : InternalMidiTransformConfiguration
{


    inline json::JsonObject GetConfigJson() noexcept
    {
    }
};


// ----

struct InternalVirtualPatchBayDestinationDefinition
{
    bool IsEnabled{ false };

    // TODO: Match Criteria

    std::vector<InternalMidiTransformConfiguration> Transforms{};



};


struct InternalVirtualPatchBayRouteDefinition
{
    winrt::guid RouteId;

    winrt::hstring Name{};
    winrt::hstring Description{};
    bool IsEnabled{ false };

    std::vector<InternalVirtualPatchBaySourceDefinition> Sources{ };
    std::vector<InternalVirtualPatchBayDestinationDefinition> Destinations{ };


    inline static InternalVirtualPatchBayRouteDefinition FromJson(_In_ json::JsonObject const& endpointEntry) noexcept
    {

    }

    inline json::JsonObject ToJson() noexcept
    {

    }
};









#endif
