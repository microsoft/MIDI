// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiClientInitializer.h"




HRESULT
CMidiClientInitializer::Initialize(
)
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        
    );

 //   DisableThreadLibraryCalls(hmodule);

    //try
    //{
    //    if (!SUCCEEDED(InstallHooks()) || !SUCCEEDED(ExtRoLoadCatalog()))
    //        return false;
    //}
    //catch (...)
    //{
    //    LOG_CAUGHT_EXCEPTION();
    //    return false;
    //}





    // TODO: set up the type resolver, detours, etc.
// Note: May need to change something about detours to support Arm64EC. TBD.

    //auto sdkLocation = GetMidiSdkPath();

    //if (!sdkLocation.has_value())
    //{
    //    // no entry for the SDK location
    //    return false;
    //}

    //m_sdkLocation = sdkLocation.value();

    //std::wstring libraryName = L"Microsoft.Windows.Devices.Midi2.dll";

    //// calling SetDllDirectory changes the loader search path. We then load
    //// the module to get it into the app's process space so that further
    //// attempts to use the DLL will simply use this. Note that if the app
    //// ships the SDK implementation DLL with the app itself, that is the
    //// version that will load, because current application directory is 
    //// always on the top of the list.
    //// 
    //// Calling the function again with NULL restores the default search
    //// order for LoadLibrary
    //// 
    //// Info on SetDllDirectoryW and its path: 
    //// https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setdlldirectoryw
    ////
    //SetDllDirectoryW(sdkLocation.value().c_str());
    //auto module = LoadLibraryW(libraryName.c_str());
    //SetDllDirectoryW(NULL);

    //// TODO: GetLastError etc.

    //if (!module)
    //{
    //    m_moduleHandle = nullptr;
    //    return false;
    //}

    //m_moduleHandle = module;

    //



    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidiClientInitializer::GetInstalledWindowsMidiServicesSdkVersion(
    PWINDOWSMIDISERVICESAPPSDKVERSION sdkVersion
)
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, sdkVersion);

    try
    {
        std::wstring versionMajor{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_MAJOR };
        std::wstring versionMinor{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_MINOR };
        std::wstring versionRevision{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_REVISION };

        // we need these three values at a minimum
        if (versionMajor.empty() || versionMinor.empty() || versionRevision.empty()) return E_FAIL;

        sdkVersion->VersionMajor = static_cast<DWORD>(std::stol(versionMajor));
        sdkVersion->VersionMinor = static_cast<DWORD>(std::stol(versionMinor));
        sdkVersion->VersionRevision = static_cast<DWORD>(std::stol(versionRevision));


        std::wstring versionDateNumber{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_DATE_NUMBER };
        std::wstring versionTimeNumber{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_TIME_NUMBER };

        if (!versionDateNumber.empty() && !versionTimeNumber.empty())
        {
            sdkVersion->VersionDateNumber = static_cast<DWORD>(std::stol(versionDateNumber));
            sdkVersion->VersionTimeNumber = static_cast<DWORD>(std::stol(versionTimeNumber));
        }
        else
        {
            sdkVersion->VersionDateNumber = 0;
            sdkVersion->VersionTimeNumber = 0;
        }

        sdkVersion->BuildSource = WINDOWS_MIDI_SERVICES_BUILD_SOURCE;
        sdkVersion->VersionName = WINDOWS_MIDI_SERVICES_BUILD_VERSION_NAME;
        sdkVersion->VersionFullString = WINDOWS_MIDI_SERVICES_BUILD_VERSION_FULL;

#if defined(_M_ARM64EC) || defined(_M_ARM64)
        // Arm64 and Arm64EC use same Arm64X binaries
        sdkVersion->Platform = MidiAppSDKPlatform::Platform_Arm64X;
#elif defined(_M_AMD64)
        // other 64 bit Intel/AMD
        sdkVersion->Platform = MidiAppSDKPlatform::Platform_x64;
#else
        // unsupported compile target architecture
#endif

        return S_OK;
    }
    catch (std::invalid_argument ex)
    {
        // if any of the numeric values is not a number
        return E_FAIL;
    }
    catch (std::out_of_range ex)
    {
        // if someone has enormous numbers in the strings
        return E_FAIL;
    }

}


HRESULT
CMidiClientInitializer::EnsureServiceAvailable()
{
    wil::com_ptr_nothrow<IMidiTransport> serviceTransport;
    wil::com_ptr_nothrow<IMidiSessionTracker> sessionTracker;

    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceTransport)));
    RETURN_IF_FAILED(serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker));

    RETURN_HR_IF_NULL(E_POINTER, sessionTracker);

    if (sessionTracker->VerifyConnectivity())
    {
        return S_OK;
    }

    return E_FAIL;
}


HRESULT
CMidiClientInitializer::Shutdown()
{
    TraceLoggingWrite(
        MidiClientInitializerTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    // Remove activation hooks
 //   RemoveHooks();


    return S_OK;
}

