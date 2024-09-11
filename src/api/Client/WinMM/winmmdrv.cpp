
#include "pch.h"
#include "MidiSrvPorts.h"

bool g_ProcessIsTerminating {false};


MMRESULT MMRESULT_FROM_HRESULT(HRESULT HResult)
{
    MMRESULT mmResult = MMSYSERR_NOERROR;

    if (FAILED(HResult))
    {
        // If this was an error that we mapped to an hresult for passing through,
        // map it back to the mmResult
        if (HRESULT_FACILITY(HResult) == FACILITY_ITF)
        {
            mmResult = HRESULT_CODE(HResult);
        }
        else
        {
            switch(HResult)
            {
                case HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES):
                    mmResult = MMSYSERR_NOMEM;
                    break;
                case E_HANDLE:
                    mmResult = MMSYSERR_BADDEVICEID;
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
    DWORD           Id,
    HDRVR           HDriver,
    WORD            Msg,
    LPARAM          Param1,
    LPARAM          Param2
)
{
    switch (Msg)
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

    return DefDriverProc( Id, HDriver, Msg, Param1, Param2 ) ;
}

DWORD APIENTRY midMessage(UINT DeviceID, UINT Msg, DWORD_PTR User, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingValue(DeviceID, "DeviceID"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(User, "User"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    // Forward the command to the global MidiPorts singleton
    return g_MidiPorts->MidMessage(DeviceID, Msg, User, Param1, Param2);
}

DWORD APIENTRY modMessage(UINT DeviceID, UINT Msg, DWORD_PTR User, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingValue(DeviceID, "DeviceID"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(User, "User"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    // Forward the command to the global MidiPorts singleton
    return g_MidiPorts->ModMessage(DeviceID, Msg, User, Param1, Param2);
}

BOOL WINAPI DllMain
(
    HINSTANCE,
    DWORD     Reason,
    LPVOID    Reserved
)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingValue(Reason, "Reason"),
        TraceLoggingValue(Reserved, "Reserved"));

    if (DLL_PROCESS_ATTACH == Reason)
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
    else if ((DLL_PROCESS_DETACH == Reason) && ( NULL != Reserved))
    {
        g_ProcessIsTerminating = true;
    }
    else if ((DLL_PROCESS_DETACH == Reason) && ( NULL == Reserved))
    {
        // if present, clean up and release the global MidiPorts singleton
        if (nullptr != g_MidiPorts)
        {
            if (FAILED(g_MidiPorts->Cleanup()))
            {
                return FALSE;
            }

            g_MidiPorts.reset();
        }
    }

    return TRUE;
}

