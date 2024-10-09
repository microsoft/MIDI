
#include "pch.h"
#include "MidiSrvPorts.h"

bool g_ProcessIsTerminating {false};


MMRESULT MMRESULT_FROM_HRESULT(HRESULT hResult)
{
    MMRESULT mmResult = MMSYSERR_NOERROR;

    if (FAILED(hResult))
    {
        // If this was an error that we mapped to an hresult for passing through,
        // map it back to the mmResult
        if (HRESULT_FACILITY(hResult) == FACILITY_ITF)
        {
            mmResult = HRESULT_CODE(hResult);
        }
        else
        {
            switch(hResult)
            {
                case HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES):
                    mmResult = MMSYSERR_NOMEM;
                    break;
                case E_HANDLE:
                    mmResult = MMSYSERR_NODRIVER;
                    break;
                case E_INVALIDARG:
                case E_POINTER:
                    mmResult = MMSYSERR_INVALPARAM;
                    break;
                case E_OUTOFMEMORY:
                    mmResult = MMSYSERR_NOMEM;
                    break;
                case E_NOINTERFACE:
                    mmResult = MMSYSERR_NOTSUPPORTED;
                    break;
                case E_FAIL:
                default:
                    mmResult = MMSYSERR_ERROR;
                    break;
            }
        }
    }

    return mmResult;
}

// MidiPorts singleton, which manages enumeration and open ports.
wil::com_ptr_nothrow<CMidiPorts> g_MidiPorts;

LRESULT CALLBACK DriverProc
(
    DWORD           id,
    HDRVR           hDriver,
    WORD            msg,
    LPARAM          param1,
    LPARAM          param2
)
{
    switch (msg)
    {
        case DRV_LOAD:
        case DRV_FREE:
        case DRV_OPEN:
        case DRV_CLOSE:
        case DRV_ENABLE:
        case DRV_DISABLE:
        case DRV_INSTALL:
            return DRV_OK;
        case DRV_QUERYCONFIGURE:
        case DRV_CONFIGURE:
        case DRV_REMOVE:
            return DRV_CANCEL;
    }

    return DefDriverProc( id, hDriver, msg, param1, param2 ) ;
}

DWORD APIENTRY midMessage(UINT deviceID, UINT msg, DWORD_PTR user, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingValue(deviceID, "deviceID"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(user, "user"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    // Forward the command to the global MidiPorts singleton
    return g_MidiPorts->MidMessage(deviceID, msg, user, param1, param2);
}

DWORD APIENTRY modMessage(UINT deviceId, UINT msg, DWORD_PTR user, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingValue(deviceId, "deviceId"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(user, "user"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    // Forward the command to the global MidiPorts singleton
    return g_MidiPorts->ModMessage(deviceId, msg, user, param1, param2);
}

BOOL WINAPI DllMain
(
    HINSTANCE,
    DWORD     reason,
    LPVOID    reserved
)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingValue(reason, "reason"),
        TraceLoggingValue(reserved, "reserved"));

    if (DLL_PROCESS_ATTACH == reason)
    {
        // Create the MidiPorts singleton, this assumes that DLL_PROCESS_ATTACH calls are serialized.
        if (nullptr == g_MidiPorts)
        {
            if (FAILED(Microsoft::WRL::MakeAndInitialize<CMidiPorts>(&g_MidiPorts)))
            {
                return FALSE;
            }

            wil::SetResultTelemetryFallback(WdmAud2TelemetryProvider::FallbackTelemetryCallback);
        }
    }
    else if ((DLL_PROCESS_DETACH == reason) && ( NULL != reserved))
    {
        g_ProcessIsTerminating = true;
    }
    else if ((DLL_PROCESS_DETACH == reason) && ( NULL == reserved))
    {
        // if present, clean up and release the global MidiPorts singleton
        if (nullptr != g_MidiPorts)
        {
            if (FAILED(g_MidiPorts->Shutdown()))
            {
                return FALSE;
            }

            g_MidiPorts.reset();
        }
    }

    return TRUE;
}

