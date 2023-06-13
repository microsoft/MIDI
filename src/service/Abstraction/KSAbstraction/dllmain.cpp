// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

CMidi2KSAbstractionModule _AtlModule;

extern "C" BOOL WINAPI
DllMain(
    HINSTANCE /* hInstance */,
    DWORD dwReason,
    LPVOID lpReserved
)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        wil::SetResultTelemetryFallback(MidiKSAbstractionTelemetryProvider::FallbackTelemetryCallback);
    }

    return _AtlModule.DllMain(dwReason, lpReserved);
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
    REFCLSID rclsid,
    REFIID riid,
    LPVOID* ppv
)
{
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
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
    BOOL bInstall,
    LPCWSTR )
{
    HRESULT hr = E_FAIL;

    if (bInstall)
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

