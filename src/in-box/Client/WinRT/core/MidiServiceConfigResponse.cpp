// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiServiceConfigResponse.h"
#include "ServiceConfig.MidiServiceConfigResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    void MidiServiceConfigResponse::InternalSetStatus(
        svc::MidiServiceConfigResponseStatus const& status)
    {
        m_status = status;
    }

    _Use_decl_annotations_
    void MidiServiceConfigResponse::InternalSetServiceError(
        uint32_t const& serviceErrorCode, 
        winrt::hstring const& serviceErrorMessage,
        json::JsonObject const& responseJson)
    {
        m_status = svc::MidiServiceConfigResponseStatus::ErrorFromService;
        m_serviceErrorCode = serviceErrorCode;
        m_serviceErrorMessage = serviceErrorMessage;
        m_responseJson = responseJson;
    }

    _Use_decl_annotations_
    void MidiServiceConfigResponse::InternalSetServiceSuccess(
        json::JsonObject const& responseJson)
    {
        m_status = svc::MidiServiceConfigResponseStatus::Success;
        m_serviceErrorCode = 0;
        m_serviceErrorMessage = L"";
        m_responseJson = responseJson;
    }
}
