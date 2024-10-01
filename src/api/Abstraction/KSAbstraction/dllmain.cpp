// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

CMidi2KSAbstractionModule _AtlModule;

extern "C" BOOL WINAPI
DllMain(
    HINSTANCE /* hInstance */,
    DWORD Reason,
    LPVOID Reserved
)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        wil::SetResultTelemetryFallback(MidiKSAbstractionTelemetryProvider::FallbackTelemetryCallback);
    }

    return _AtlModule.DllMain(Reason, Reserved);
}

_Use_decl_annotations_
STDAPI
DllCanUnloadNow(void)
{
    auto &module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();
    return module.Terminate() ? _AtlModule.DllCanUnloadNow() : S_FALSE;
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

