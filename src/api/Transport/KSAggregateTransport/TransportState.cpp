// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


TransportState::TransportState() = default;
TransportState::~TransportState() = default;

TransportState& TransportState::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static TransportState current;

    return current;
}


HRESULT
TransportState::ConstructEndpointManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiEndpointManager>(&m_endpointManager));

    return S_OK;
}


HRESULT
TransportState::ConstructConfigurationManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiConfigurationManager>(&m_configurationManager));

    return S_OK;
}
