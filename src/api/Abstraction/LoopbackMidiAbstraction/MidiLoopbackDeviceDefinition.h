// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

// This information is provided by the configuration manager

struct MidiLoopbackDeviceDefinition
{
    std::wstring AssociationId{};

    std::wstring EndpointAName{};
    std::wstring EndpointADescription{};
    std::wstring EndpointAUniqueIdentifier{};

    std::wstring EndpointBName{};
    std::wstring EndpointBDescription{};
    std::wstring EndpointBUniqueIdentifier{};

};
