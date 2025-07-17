// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

// --------------------------------------------------------------------------
// Had to move this all to the DownscalerMidiTransform cpp file because the 
// header-only implementation of libmidi2 was causing duplicate definitions
// of the same types, and this was the only reasonable way around it
// --------------------------------------------------------------------------


//#include "Midi2.UmpProtocolDownscalerMidiTransform.h"
//
//_Use_decl_annotations_
//HRESULT
//CMidi2UmpProtocolDownscalerTransform::Activate(
//    REFIID iid,
//    void **activatedInterface
//)
//{
//    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);
//
//    if (__uuidof(IMidiDataTransform) == iid)
//    {
//        TraceLoggingWrite(
//            MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
//            MIDI_TRACE_EVENT_INFO,
//            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//            TraceLoggingPointer(this, "this"),
//            TraceLoggingWideString(L"IMidiDataTransform", MIDI_TRACE_EVENT_INTERFACE_FIELD)
//            );
//
//        wil::com_ptr_nothrow<IMidiDataTransform> midiTransform;
//        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2UmpProtocolDownscalerMidiTransform>(&midiTransform));
//        *activatedInterface = midiTransform.detach();
//    }
//
//    else
//    {
//        return E_NOINTERFACE;
//    }
//
//    return S_OK;
//}

