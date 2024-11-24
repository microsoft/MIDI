#include "pch.h"

// TODO: Move trace/telemetry initialization here and clean up the macros

HMODULE g_sdkRuntimeImplementationModuleHandle;

BOOL WINAPI DllMain(HINSTANCE hmodule, DWORD reason, LPVOID /*lpvReserved*/)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // TODO: validate Windows version

    g_sdkRuntimeImplementationModuleHandle = hmodule;

    if (reason == DLL_PROCESS_ATTACH)
    {
        wil::SetResultTelemetryFallback(Midi2SdkTelemetryProvider::FallbackTelemetryCallback);

        DisableThreadLibraryCalls(hmodule);

        // detours initialization is all done in the COM component
        // but we want only a single instance per-process
        if (g_clientInitializer == nullptr)
        {
            g_clientInitializer = winrt::make<MidiClientInitializer>();
        }

    }

    if (reason == DLL_PROCESS_DETACH)
    {
    //    g_clientInitializer = nullptr;
    }

    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return true;
}

_Use_decl_annotations_
STDAPI
DllCanUnloadNow()
{
    // TODO: See if the initializer has any references. If not, then forward to the WinRT function

    return WINRT_CanUnloadNow();
}

_Use_decl_annotations_
STDAPI
DllGetClassObject(GUID const& clsid, GUID const& iid, void** result)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this")
    );

    try
    {
        RETURN_HR_IF(E_POINTER, IsBadWritePtr(result, sizeof(void*)));

        *result = nullptr;

        if (clsid == __uuidof(MidiClientInitializer))
        {
            return winrt::make<MidiClientInitializerFactory>()->QueryInterface(iid, result);
        }

        // TODO: Is this going to fail for apps like MultitrackStudio which use the underlying COM interfaces for the WinRT classes?
        // not a supported class
        return winrt::hresult_class_not_available().to_abi();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}


std::wstring GetThisDllFullPath()
{
    std::vector<wchar_t> moduleFileNameBuffer;

    size_t countCharsCopied{ 0 };
    do
    {
        moduleFileNameBuffer.resize(countCharsCopied + MAX_PATH);
        countCharsCopied = static_cast<size_t>(GetModuleFileName(
            g_sdkRuntimeImplementationModuleHandle, 
            moduleFileNameBuffer.data(), 
            static_cast<DWORD>(moduleFileNameBuffer.size())));

    } while (countCharsCopied >= moduleFileNameBuffer.size());

    std::wstring moduleFileName(moduleFileNameBuffer.begin(), moduleFileNameBuffer.end());

    return moduleFileName;
}


STDAPI
DllRegisterServer(void)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    auto moduleFileName = GetThisDllFullPath();
    auto initializerClsid = internal::GuidToString(__uuidof(MidiClientInitializer));
    auto initializerIID = internal::GuidToString(__uuidof(IMidiClientInitializer));
    auto initializerProgId = std::wstring{ MIDI_CLIENT_INITIALIZER_PROG_ID };

    // Register the initializer coclass -----------------------------
    auto initializerClsidKeyName = L"CLSID\\" + initializerClsid;

    wil::shared_hkey hkeyInitializerClsidRoot;
    RETURN_IF_FAILED(wil::reg::create_shared_key_nothrow(HKEY_CLASSES_ROOT, initializerClsidKeyName.c_str(), hkeyInitializerClsidRoot, wil::reg::key_access::readwrite));
    RETURN_IF_FAILED(wil::reg::set_value_nothrow(hkeyInitializerClsidRoot.get(), nullptr, L"MidiClientInitializer class"));

    wil::shared_hkey hkeyInitializerInprocServer32;
    RETURN_IF_FAILED(wil::reg::create_shared_key_nothrow(hkeyInitializerClsidRoot.get(), L"InprocServer32", hkeyInitializerInprocServer32, wil::reg::key_access::readwrite));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(hkeyInitializerInprocServer32.get(), nullptr, moduleFileName.c_str()));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(hkeyInitializerInprocServer32.get(), L"ThreadingModel", L"Both"));

    // Register the progid for the coclass --------------------------
    wil::shared_hkey hkeyInitializerProgIdRoot;
    RETURN_IF_FAILED(wil::reg::create_shared_key_nothrow(HKEY_CLASSES_ROOT, initializerProgId.c_str(), hkeyInitializerProgIdRoot, wil::reg::key_access::readwrite));

    wil::shared_hkey hkeyInitializerProgIdClsid;
    RETURN_IF_FAILED(wil::reg::create_shared_key_nothrow(hkeyInitializerProgIdRoot.get(), std::wstring{ L"CLSID" }.c_str(), hkeyInitializerProgIdClsid, wil::reg::key_access::readwrite));
    RETURN_IF_FAILED(wil::reg::set_value_string_nothrow(hkeyInitializerProgIdClsid.get(), nullptr, initializerClsid.c_str()));

    // Register the initializer interface ---------------------------



    // register the class factory


    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(moduleFileName.c_str(), "module file name"),
        TraceLoggingWideString(MIDI_CLIENT_INITIALIZER_PROG_ID, "prog id"),
        TraceLoggingWideString(initializerClsid.c_str(), "initializer clsid")
        );


    return S_OK;
}

STDAPI
DllUnregisterServer(void)
{
    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // remove the clsid
    auto initializerClsid = internal::GuidToString(__uuidof(MidiClientInitializer));
    wil::shared_hkey clsidRoot;
    RETURN_IF_FAILED(wil::reg::open_shared_key_nothrow(HKEY_CLASSES_ROOT, L"CLSID", clsidRoot, wil::reg::key_access::readwrite));

    auto view = wil::reg::reg_view_details::reg_view_nothrow(clsidRoot.get());
    view.delete_tree(initializerClsid.c_str());

    // remove the prog id
    auto initializerProgId = std::wstring{ MIDI_CLIENT_INITIALIZER_PROG_ID };
    wil::shared_hkey hkeyInitializerProgIdRoot;
    RETURN_IF_FAILED(wil::reg::open_shared_key_nothrow(HKEY_CLASSES_ROOT, nullptr, hkeyInitializerProgIdRoot, wil::reg::key_access::readwrite));

    auto progIdview = wil::reg::reg_view_details::reg_view_nothrow(hkeyInitializerProgIdRoot.get());
    progIdview.delete_tree(initializerProgId.c_str());



    TraceLoggingWrite(
        Midi2SdkTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
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