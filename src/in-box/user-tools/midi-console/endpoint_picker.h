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

    struct GroupPickerResult
    {
        bool Canceled{ true };
        uint8_t GroupIndex{ 0 };
    };

    // Groups the endpoint declares it uses in the given direction, labeled with the function
    // block or group terminal block that claims each one. An endpoint that declares nothing
    // usable falls back to all sixteen groups.
    GroupPickerResult PickGroup(
        _In_ std::string_view prompt,
        _In_ std::string const& endpointDeviceId,
        _In_ bool wantMessageSource);
}
