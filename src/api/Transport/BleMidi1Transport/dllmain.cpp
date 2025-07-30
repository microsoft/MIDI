// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

CMidi2Ble1MidiTransportModule _AtlModule;

extern "C" BOOL WINAPI
DllMain(
    HINSTANCE /* hInstance */,
    DWORD Reason,
    LPVOID Reserved
)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        wil::SetResultTelemetryFallback(MidiBle1MidiTransportTelemetryProvider::FallbackTelemetryCallback);
    }

    return _AtlModule.DllMain(Reason, Reserved);
}

_Use_decl_annotations_
STDAPI
DllCanUnloadNow(void)
{
    return _AtlModule.DllCanUnloadNow();
}

_Use_decl_annotations_
STDAPI
DllGetClassObject(
    REFCLSID Clsid,
    REFIID Riid,
    LPVOID* Object
)
{
    return _AtlModule.DllGetClassObject(Clsid, Riid, Object);
}

STDAPI
DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    HRESULT hr = _AtlModule.DllRegisterServer(FALSE);
    return hr;
}

STDAPI
DllUnregisterServer(void)
{
    HRESULT hr = _AtlModule.DllUnregisterServer(FALSE);
    return hr;
}

_Use_decl_annotations_
STDAPI
DllInstall(
    BOOL Install,
    LPCWSTR )
{
    HRESULT hr = E_FAIL;

    if (Install)
    {
        hr = DllRegisterServer();
        if (FAILED(hr))
        {
            DllUnregisterServer();
        }
    }
    else
    {
        hr = DllUnregisterServer();
    }

    return hr;
}

