// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    struct EndpointPickerResult
    {
        bool Canceled{ true };
        std::string EndpointDeviceId;
        std::string EndpointName;
    };

    EndpointPickerResult PickEndpoint(_In_ std::string_view prompt);

    // Every endpoint command funnels through here: use the supplied id, or show the picker.
    // Returns false when the user canceled or the console cannot prompt.
    bool ResolveEndpointDeviceId(_Inout_ std::string& endpointDeviceId, _Out_ std::string& endpointName);
}
