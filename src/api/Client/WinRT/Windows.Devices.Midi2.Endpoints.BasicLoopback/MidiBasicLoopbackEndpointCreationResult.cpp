// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiBasicLoopbackEndpointCreationResult.h"
#include "MidiBasicLoopbackEndpointCreationResult.g.cpp"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    _Use_decl_annotations_
    void MidiBasicLoopbackEndpointCreationResult::InternalSetSuccess(
        winrt::guid associationId, 
        bloop::MidiBasicLoopbackEntry& entry) noexcept
    {
        m_success = true;
        m_associationId = associationId;
        m_createdLoopbackEntry = entry;
    }

    _Use_decl_annotations_
    void MidiBasicLoopbackEndpointCreationResult::InternalSetError(
        winrt::guid associationId, 
        bloop::MidiBasicLoopbackEndpointCreationResultErrorCode errorCode, 
        winrt::hstring const& errorMessage) noexcept
    {
        m_success = false;
        m_associationId = associationId;
        m_errorCode = errorCode;
        m_errorMessage = errorMessage;
    }



}
