
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
                    hResult = MMSYSERR_NOMEM;
                    break;
                case E_HANDLE:
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
    LPARAM          lParam1,
    LPARAM          lParam2
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

    return DefDriverProc( id, hDriver, msg, lParam1, lParam2 ) ;
}

DWORD APIENTRY midMessage(UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    // Forward the command to the global MidiPorts singleton
    return g_MidiPorts->MidMessage(uDeviceID, uMsg, dwUser, dwParam1, dwParam2);
}

DWORD APIENTRY modMessage(UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    // Forward the command to the global MidiPorts singleton
    return g_MidiPorts->ModMessage(uDeviceID, uMsg, dwUser, dwParam1, dwParam2);
}

BOOL WINAPI DllMain
(
    HINSTANCE,
    DWORD     Reason,
    LPVOID    Reserved
)
{
    if (DLL_PROCESS_ATTACH == Reason)
    {
        // Create the MidiPorts singleton, this assumes that DLL_PROCESS_ATTACH calls are serialized.
        if (nullptr == g_MidiPorts)
        {
            if (FAILED(Microsoft::WRL::MakeAndInitialize<CMidiPorts>(&g_MidiPorts)))
            {
                return FALSE;
            }
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

