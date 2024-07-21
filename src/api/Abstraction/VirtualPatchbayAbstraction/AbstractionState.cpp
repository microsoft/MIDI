// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


AbstractionState::AbstractionState() = default;
AbstractionState::~AbstractionState() = default;

AbstractionState& AbstractionState::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static AbstractionState current;

    return current;
}



HRESULT
AbstractionState::ConstructEndpointManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2VirtualPatchBayEndpointManager>(&m_endpointManager));

    return S_OK;
}


HRESULT
AbstractionState::ConstructConfigurationManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2VirtualPatchBayConfigurationManager>(&m_configurationManager));

    return S_OK;
}
