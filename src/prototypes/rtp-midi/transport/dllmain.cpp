// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "pch.h"

CMidi2RtpMidiTransportModule _AtlModule;

extern "C" BOOL WINAPI
DllMain(
    HINSTANCE,
    DWORD reason,
    LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wil::SetResultTelemetryFallback(MidiRtpMidiTransportTelemetryProvider::FallbackTelemetryCallback);
    }

    return _AtlModule.DllMain(reason, reserved);
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
    REFCLSID clsid,
    REFIID riid,
    LPVOID* object)
{
    return _AtlModule.DllGetClassObject(clsid, riid, object);
}

STDAPI
DllRegisterServer(void)
{
    return _AtlModule.DllRegisterServer(FALSE);
}

STDAPI
DllUnregisterServer(void)
{
    return _AtlModule.DllUnregisterServer(FALSE);
}

_Use_decl_annotations_
STDAPI
DllInstall(
    BOOL install,
    LPCWSTR)
{
    HRESULT hr = E_FAIL;

    if (install)
    {
        hr = DllRegisterServer();
        if (FAILED(hr)) DllUnregisterServer();
    }
    else
    {
        hr = DllUnregisterServer();
    }

    return hr;
}
