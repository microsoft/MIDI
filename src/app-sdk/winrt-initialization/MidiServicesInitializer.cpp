// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiServicesInitializer.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization::implementation
{
    std::wstring MidiServicesInitializer::m_sdkLocation{};
    HMODULE MidiServicesInitializer::m_moduleHandle{ nullptr };


    foundation::Uri MidiServicesInitializer::GetLatestDesktopAppSdkRuntimeReleaseInstallerUri()
    {
        foundation::Uri installerUri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller");

        return installerUri;
    }

    //foundation::Uri MidiServicesInitializer::GetLatestSettingsAppReleaseInstallerUri()
    //{
    //    foundation::Uri installerUri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller");

    //    return installerUri;
    //}

    //foundation::Uri MidiServicesInitializer::GetLatestConsoleAppReleaseInstallerUri()
    //{
    //    foundation::Uri installerUri(L"https://aka.ms/MidiServicesLatestConsoleInstaller");

    //    return installerUri;
    //}


    std::optional<std::wstring> GetMidiSdkPath()
    {
#if defined(_M_ARM64EC) || defined(_M_ARM64)
        // Arm64 and Arm64EC use same Arm64X binaries
        const std::optional<std::wstring> sdkLocation = wil::reg::try_get_value_string(HKEY_LOCAL_MACHINE, MIDI_ROOT_ENDPOINT_APP_SDK_REG_KEY, MIDI_APP_SDK_ARM64X_REG_VALUE);
#elif defined(_M_AMD64)
        // other 64 bit Intel/AMD
        const std::optional<std::wstring> sdkLocation = wil::reg::try_get_value_string(HKEY_LOCAL_MACHINE, MIDI_ROOT_ENDPOINT_APP_SDK_REG_KEY, MIDI_APP_SDK_X64_REG_VALUE);
#else
        // unsupported compile target architecture
#endif

        return sdkLocation;
    }


    bool MidiServicesInitializer::IsCompatibleDesktopAppSdkRuntimeInstalled()
    {
        // get location of runtime. If the reg key isn't present, return failure
        auto sdkLocation = GetMidiSdkPath();

        if (sdkLocation.has_value())
        {
            return true;
        }

        return false;
    }

    bool MidiServicesInitializer::IsOperatingSystemSupported()
    {

        // TODO

        return false;
    }

    // this should be called only by desktop applications, not by packaged apps that
    // will contain the SDK binaries in the package
    bool MidiServicesInitializer::InitializeDesktopAppSdkRuntime()
    {
        if (!IsCompatibleDesktopAppSdkRuntimeInstalled())
        {
            return false;
        }

        // TODO: set up the type resolver, detours, etc.
        // Note: May need to change something about detours to support Arm64EC. TBD.

        auto sdkLocation = GetMidiSdkPath();

        if (!sdkLocation.has_value())
        {
            // no entry for the SDK location
            return false;
        }

        m_sdkLocation = sdkLocation.value();

        std::wstring libraryName = L"Microsoft.Windows.Devices.Midi2.dll";
        
        // calling SetDllDirectory changes the loader search path. We then load
        // the module to get it into the app's process space so that further
        // attempts to use the DLL will simply use this. Note that if the app
        // ships the SDK implementation DLL with the app itself, that is the
        // version that will load, because current application directory is 
        // always on the top of the list.
        // 
        // Calling the function again with NULL restores the default search
        // order for LoadLibrary
        // 
        // Info on SetDllDirectoryW and its path: 
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setdlldirectoryw
        //
        SetDllDirectoryW(sdkLocation.value().c_str());
        auto module = LoadLibraryW(libraryName.c_str());
        SetDllDirectoryW(NULL);

        // TODO: GetLastError etc.

        if (!module)
        {
            m_moduleHandle = nullptr;
            return false;
        }

        m_moduleHandle = module;

        return true;
    }


    bool MidiServicesInitializer::ShutdownDesktopAppSdkRuntime()
    {
        if (m_moduleHandle)
        {
            if (FreeLibrary(m_moduleHandle))
            {
                m_moduleHandle = nullptr;
                return true;
            }
        }

        return false;
    }

    // returns True if the MIDI Service is available on this PC
    bool MidiServicesInitializer::EnsureServiceAvailable()
    {
        try
        {
            auto serviceAbstraction = winrt::create_instance<IMidiTransport>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            // winrt::try_create_instance indicates failure by returning an empty com ptr
            // winrt::create_instance indicates failure with an exception
            if (serviceAbstraction == nullptr)
            {
                LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error contacting service. Service abstraction is nullptr", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );                
                
                return false;
            }

            winrt::com_ptr<IMidiSessionTracker> tracker;

            auto sessionTrackerResult = serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&tracker);
            if (FAILED(sessionTrackerResult))
            {
                LOG_IF_FAILED(sessionTrackerResult);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failure hresult received activating interface", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(sessionTrackerResult, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }

            auto verifyConnectivityResult = tracker->VerifyConnectivity();

            if (!verifyConnectivityResult)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failure received verifying connectivity", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return false;
            }

            return true;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error contacting service. It may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );


            // winrt::create_instance fails by throwing an exception
            return false;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error contacting service. It may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // winrt::create_instance fails by throwing an exception
            return false;
        }
    }
}
