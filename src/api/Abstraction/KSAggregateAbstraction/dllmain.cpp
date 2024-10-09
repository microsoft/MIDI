// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

CMidi2KSAggregateAbstractionModule _AtlModule;

extern "C" BOOL WINAPI
DllMain(
    HINSTANCE /* hInstance */,
    DWORD reason,
    LPVOID reserved
)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wil::SetResultTelemetryFallback(MidiKSAggregateAbstractionTelemetryProvider::FallbackTelemetryCallback);
    }

    return _AtlModule.DllMain(reason, reserved);
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
    REFCLSID clsid,
    REFIID riid,
    LPVOID* object
)
{
    return _AtlModule.DllGetClassObject(clsid, riid, object);
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
    BOOL install,
    LPCWSTR )
{
    HRESULT hr = E_FAIL;

    if (install)
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

