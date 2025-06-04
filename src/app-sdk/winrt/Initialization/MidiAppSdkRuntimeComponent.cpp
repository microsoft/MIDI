// Copyright (c) Microsoft Corporation and Contributors
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================
// This file modified from
// https://github.com/microsoft/xlang/tree/master/src/UndockedRegFreeWinRT
// which retains its own copyrights as described in the xlang repo

#include "pch.h"

#include "MidiAppSdkRuntimeComponent.h"

extern HMODULE g_sdkRuntimeImplementationModuleHandle;

//HRESULT 
//MidiAppSdkRuntimeComponent::LoadModuleIfNeeded()
//{
//    TraceLoggingWrite(
//        Midi2SdkTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingPointer(this, "this"),
//        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
//        TraceLoggingWideString(module_name.c_str(), "module_name")
//    );
//
//    return S_OK;
//
//    //// TODO: we could further optimize this because all the SDK types will have the same proc address for this
//    ////this->get_activation_factory = (activation_factory_type)GetProcAddress(g_sdkRuntimeImplementationModuleHandle, "DllGetActivationFactory");
//
//
//
//    //if (this->get_activation_factory == nullptr)
//    //{
//    //    auto hr = HRESULT_FROM_WIN32(GetLastError());
//
//    //    TraceLoggingWrite(
//    //        Midi2SdkTelemetryProvider::Provider(),
//    //        MIDI_TRACE_EVENT_INFO,
//    //        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//    //        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//    //        TraceLoggingPointer(this, "this"),
//    //        TraceLoggingWideString(L"GetProcAddress(DllGetActivationFactory) failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
//    //        TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD),
//    //        TraceLoggingWideString(module_name.c_str(), "module_name")
//    //    );
//
//    //    return hr;
//    //}
//
//    //return (this->get_activation_factory != nullptr) ? S_OK : E_FAIL;
//}


_Use_decl_annotations_
HRESULT 
MidiAppSdkRuntimeComponent::GetActivationFactory(
    HSTRING className, 
    REFIID  iid, 
    void** factory
)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

 //   RETURN_IF_FAILED(LoadModuleIfNeeded());

    IActivationFactory* ifactory = nullptr;
    //HRESULT hr = this->get_activation_factory(className, &ifactory);

    HRESULT hr = WINRT_GetActivationFactory(className, (void**)(&ifactory));


    // optimize for IActivationFactory?
    if (SUCCEEDED(hr))
    {
        hr = ifactory->QueryInterface(iid, factory);
        ifactory->Release();

        if (!SUCCEEDED(hr))
        {
            LOG_IF_FAILED(hr);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"QueryInterface failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(module_name.c_str(), "module_name"),
                TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );
        }
    }
    else
    {
        LOG_IF_FAILED(hr);

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"get_activation_factory failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(module_name.c_str(), "module_name"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );
    }

    return hr;
}