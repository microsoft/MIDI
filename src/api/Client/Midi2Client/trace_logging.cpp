// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include <pch.h>

#include <winmeta.h>

namespace Windows::Devices::Midi2::Internal
{

    // Windows.Devices.Midi2.Api hashed tracing guid: 5c055d9e-0ac2-58ee-f647-c1f00339a6ec

    TRACELOGGING_DEFINE_PROVIDER(
        g_hLoggingProvider,
        TRACELOGGING_PROVIDER_NAME,                         
        // {5c055d9e-0ac2-58ee-f647-c1f00339a6ec}
        (0x5c055d9e, 0x0ac2, 0x58ee, 0xf6, 0x47, 0xc1, 0xf0, 0x03, 0x39, 0xa6, 0xec));  



    void WINAPI LoggingProviderEnabledCallback(
        _In_      LPCGUID /*sourceId*/,
        _In_      ULONG /*isEnabled*/,
        _In_      UCHAR /*level*/,
        _In_      ULONGLONG /*matchAnyKeyword*/,
        _In_      ULONGLONG /*matchAllKeywords*/,
        _In_opt_  PEVENT_FILTER_DESCRIPTOR /*filterData*/,
        _In_opt_  PVOID /*callbackContext*/)
    {
        //g_IsLoggingProviderEnabled = !!isEnabled;
        //g_LoggingProviderLevel = level;
        //g_LoggingProviderMatchAnyKeyword = matchAnyKeyword;
    }


    void RegisterTraceLogging()
    {
        // HRESULT hr = S_OK;

        TraceLoggingRegisterEx(g_hLoggingProvider, LoggingProviderEnabledCallback, nullptr);

        ////Generate the ActivityId used to track the session
        //hr = CoCreateGuid(&g_LoggingProviderActivityId);

        //if (FAILED(hr))
        //{
        //    TraceLoggingWriteActivity(
        //        g_hLoggingProvider,
        //        "CreateGuidError",
        //        nullptr,
        //        nullptr,
        //        TraceLoggingHResult(hr));

        //    g_LoggingProviderActivityId = GUID_NULL;
        //};
    }


    void UnregisterTraceLogging()
    {
        TraceLoggingUnregister(g_hLoggingProvider);
    }



    // may want to allow passing in a keyword so we can be finer-grained
    _Use_decl_annotations_
    void LogHresultError(
        const char* location, 
        const wchar_t* message, 
        winrt::hresult_error const& ex) noexcept
    {
        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.HresultError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API),
            TraceLoggingHResult(ex.code(), "HRESULT"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message"),
            TraceLoggingWideString(ex.message().c_str(), "Error")
        );
    }

    _Use_decl_annotations_
    void LogGeneralError(
        const char* location, 
        const wchar_t* message) noexcept
    {
        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.GeneralError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }

    _Use_decl_annotations_
    void LogUmpDataValidationError(
        const char* location, 
        const wchar_t* message, 
        const uint32_t firstWord, 
        const uint64_t timestamp) noexcept
    {
        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.UmpDataValidationError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API),
            TraceLoggingHexUInt32(firstWord, "UMPFirstWord"),
            TraceLoggingUInt64(timestamp, "MIDITimestamp"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }

    _Use_decl_annotations_
    void LogUmpSizeValidationError(
        const char* location, 
        const wchar_t* message, 
        const uint32_t providedSizeInWords, 
        const uint64_t timestamp) noexcept
    {
        TraceLoggingWrite(
            g_hLoggingProvider,
            "MIDI.UmpSizeValidationError",
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TRACE_KEYWORD_API),
            TraceLoggingHexUInt32(providedSizeInWords, "ProvidedSizeInWords"),
            TraceLoggingUInt64(timestamp, "MIDITimestamp"),
            TraceLoggingString(location, "Location"),
            TraceLoggingWideString(message, "Message")
        );
    }


}