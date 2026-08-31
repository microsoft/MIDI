// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI2_MESSAGE_TYPE_FILTER_TRANSFORM_IMPLEMENTATION_H
#define MIDI2_MESSAGE_TYPE_FILTER_TRANSFORM_IMPLEMENTATION_H

#include "pch.h"



using namespace winrt::Windows::Devices::Enumeration;
//
// Despite the name, this does both downscaling and upscaling
// New name should really be CMidi2UmpProtocolMidiTransform
//
class CMidi2MessageTypeFilterTransformImplementation :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(
        _In_ LPCWSTR,
        _In_ PTRANSFORMCREATIONPARAMS,
        _In_ DWORD*,
        _In_opt_ IMidiCallback*,
        _In_ LONGLONG,
        _In_ IMidiDeviceManager*));

    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Shutdown)();

private:
    wil::critical_section m_sendLock;

    std::wstring m_endpointDeviceInterfaceId;
    wil::com_ptr_nothrow<IMidiCallback> m_callback;
    LONGLONG m_context;


};

#endif
