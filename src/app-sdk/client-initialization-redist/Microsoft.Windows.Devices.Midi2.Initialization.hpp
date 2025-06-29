// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK. This file may be compiled
// into your applications to bootstrap the service and SDK without the need
// for additional binaries shipped local to the app.
// Further information: https://aka.ms/midi
// ============================================================================
// the code in here is a bit verbose because we can't rely on helpers like wil
// to simplify things. If you are already using wil, there are suggestions in
// line in the code.
// 
// You should feel free to modify this code to suit your application. If you 
// add additional features, find bugs, or otherwise have recommended changes,
// we are happy to take PRs at https://aka.ms/midirepo

#pragma once

#ifndef MICROSOFT_WINDOWS_DEVICES_MIDI2_INITIALIZATION_HPP
#define MICROSOFT_WINDOWS_DEVICES_MIDI2_INITIALIZATION_HPP

#include <string>           // for wstring
#include <combaseapi.h>     // for COM activation CoCreateInstance etc.


#define SAFE_COTASKMEMFREE(p) \
    if (NULL != p) { \
        CoTaskMemFree(p); \
        (p) = NULL; \
    }

namespace Microsoft::Windows::Devices::Midi2::Initialization
{
    typedef enum
    {
        Platform_x64 = 1,
        //    Platform_Arm64 = 2,
        //    Platform_Arm64EC = 3,
        Platform_Arm64X = 4,
    } MidiAppSDKPlatform;

    struct __declspec(uuid("8087b303-d551-bce2-1ead-a2500d50c580")) IMidiClientInitializer : ::IUnknown
    {
        // initialize the SDK runtime, and set up WinRT detours
        //STDMETHOD(Initialize)() = 0;

        // clean up
        //STDMETHOD(Shutdown)() = 0;

        // returns the SDK version info. Supply nullptr for arguments you don't care about
        STDMETHOD(GetInstalledWindowsMidiServicesSdkVersion)(
            MidiAppSDKPlatform* buildPlatform,
            DWORD* versionMajor,
            DWORD* versionMinor,
            DWORD* versionRevision,
            DWORD* versionBuildNumber,

            LPWSTR* buildSource,
            LPWSTR* versionName,
            LPWSTR* versionFullString
            ) = 0;

        // demand-starts the service if present
        STDMETHOD(EnsureServiceAvailable)() = 0;
    };

    // this is the COM entry point into the SDK runtime. It's here just to get the UUID using __uuidof
    struct __declspec(uuid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3")) MidiClientInitializerUuid
    {
    };

    // we use this to test service connectivity. It's here just to get the UUID using __uuidof
    // The MidiSrvTransport is installed with the MIDI Service, not the SDK, so its presence
    // tells us the service has been installed on this PC
    struct __declspec(uuid("2BA15E4E-5417-4A66-85B8-2B2260EFBC84")) MidiSrvTransportUuid : ::IUnknown
    {
    };

    class MidiDesktopAppSdkInitializer
    {
    private:
        // if using wil, you can use wil::com_ptr_nothrow<IMidiClientInitializer> here
        // and then remove the calls to AddRef/Release. Similar if using winrt::com_ptr
        // or even the ATL CComPtr. Note that std smart pointers do not call the COM
        // specific functions, so they are not of real benefit here.
        // for similar reasons, this pointer must not be allowed to escape this
        // class, or else the COM component will not be properly shut down.
        IMidiClientInitializer* m_initializer{ nullptr };

    public:
        // this URL will not be changed, but the underlying location it redirects to
        // may change. The aka will always point to a page where the latest SDK runtime 
        // can be installed, so please use this as the download link for your customers
        const std::wstring LatestMidiAppSdkDownloadUrl{ L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller" };

        // If you want to check to see that you are on a supported operating system, this
        // is the appropriate clal to make. This is more feature-focused as opposed to 
        // checking for a specific Windows version. We simply try to create the main
        // transport to communicate with the MIDI Service. If it works, the service is
        // installed and registered. If not, then the service is not present/registered

        // You need to initialize COM or WinRT before calling this method. Example:
        // winrt::init_apartment() or CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        bool IsServiceInstalled()
        {
            // if you use a ref-counting pointer from wil, winrt, ATL, etc. feel
            // free to use it here and remove the Release() call below.
            IUnknown* servicePointer{ nullptr };

            auto hr = CoCreateInstance(
                __uuidof(MidiSrvTransportUuid),
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&servicePointer)
            );

            if (SUCCEEDED(hr))
            {
                // this shouldn't happen, but we should guard anyway
                if (servicePointer == nullptr) return false;

                // Windows MIDI Services appears to be installed. Because this
                // succeeded, we need to release the COM reference. To avoid this kind
                // of logic, use a wil::com_ptr_nothrow, a winrt::com_ptr, or an ATL CComPtr
                servicePointer->Release();
                servicePointer = nullptr;
                return true;
            }
            else if (hr == REGDB_E_CLASSNOTREG)
            {
                // Class not registered. Windows MIDI Services does NOT appear to be installed
                // no need to release here because CoCreateInstance failed
                servicePointer = nullptr;
                return false;
            }
            else
            {
                // Other error. Unexpected.
                // no need to release here because CoCreateInstance failed
                servicePointer = nullptr;
                return false;
            }
        }

        // this must be called only *after* initializing the SDK Runtime
        bool EnsureServiceAvailable()
        {
            // initialize the SDK runtime before calling this function
            if (m_initializer == nullptr) return false;

            // attempt to activate service
            if (SUCCEEDED(m_initializer->EnsureServiceAvailable()))
            {
                return true;
            }
            else
            {
                return false;
            }
        }


        // IMPORTANT caller note: this is assuming that the caller has made the appropriate
        // winrt::init_apartment() (C++/WinRT) or Windows::Foundation::Initialize 
        // call on this calling thread before calling this function. Do not call 
        // CoInitializeEx because we're also using WinRT and the other two approaches 
        // do the additional initialization we require.
        //
        // In the future, we may add either an overload to this or internally handle
        // version differences so that we can have side-by-side major revisions of the 
        // SDK, if needed. We're leaving that out for now, and will ensure backwards
        // compatibility.
        bool InitializeSdkRuntime()
        {
            // check if already initialized
            if (m_initializer != nullptr) return false;

            // creating the initializer will also initialize it if we are the first
            // consumer in this process.
            if (SUCCEEDED(CoCreateInstance(
                    __uuidof(MidiClientInitializerUuid),
                    NULL,
                    CLSCTX::CLSCTX_INPROC_SERVER | CLSCTX::CLSCTX_FROM_DEFAULT_CONTEXT,
                    __uuidof(IMidiClientInitializer),
                    reinterpret_cast<void**>(&m_initializer)
                )))
            {
                if (m_initializer != nullptr)
                {
                    return true;
                }
                else
                {
                    // not something that should happen. If CoCreateInstance succeeded, then
                    // m_initializerComponent will not be nullptr
                    return false;
                }
            }
            else
            {
                // the MidiClientInitializer is not available. That means the SDK has not
                // been installed. The calling application should prompt the user to download
                // and install the Windows MIDI Services SDK runtime from AppSDKDownloadURLString
                // 
                // Note that this approach is consistent with the Windows App SDK Approach 
                // recently adopted in Windows, except we don't pop up UI here, and instead
                // prefer to leave that up to the app to do in its own consistent way.
                m_initializer = nullptr;
                return false;
            }
        }

        // If you know your application requires a minimum version of the SDK, because maybe you use features
        // added later, or you know there was an important bug fix, you can use this function, after initialization
        // to check for a minimum required version. Use the provided URL to download the latest runtime.
        // You will need to shutdown the SDK before the customer can replace it.
        // The SDK version you compile against is the NuGet package major/minor (and optionally) revision.
        bool CheckForMinimumRequiredSdkVersion(
            DWORD minRequiredVersionMajor, 
            DWORD minRequiredVersionMinor, 
            DWORD minRequiredVersionRevision)
        {
            if (m_initializer != nullptr)
            {
                DWORD installedVersionMajor{ 0 };
                DWORD installedVersionMinor{ 0 };
                DWORD installedVersionRevision{ 0 };

                if (SUCCEEDED(m_initializer->GetInstalledWindowsMidiServicesSdkVersion(
                    nullptr,                    // build platform
                    &installedVersionMajor,     // major
                    &installedVersionMinor,     // minor
                    &installedVersionRevision,  // revision
                    nullptr,                    // build number
                    nullptr,                    // buildSource string. Remember to cotaskmemfree if provided
                    nullptr,                    // versionName string. Remember to cotaskmemfree if provided
                    nullptr                     // versionFullString string. Remember to cotaskmemfree if provided
                )))
                {
                    if (minRequiredVersionMajor > installedVersionMajor) return false;
                    if (minRequiredVersionMinor > installedVersionMinor) return false;
                    if (minRequiredVersionRevision > installedVersionRevision) return false;

                    return true;
                }

                return false;
            }
            else
            {
                // the SDK has not been initialized, so we cannot get a version from it.
                // the caller should call InitializeSdkRuntime first
                return false;
            }
        }


        // TODO: This should be named "ReleaseSdkRuntime" or similar. 
        // and it should also unload the DLL
        void ShutdownSdkRuntime()
        {
            if (m_initializer != nullptr)
            {

                // release the COM component. After the last client releases the
                // initializer referenced, it will clean up the hooks.

                m_initializer->Release();   // if using wil::com_ptr_nothrow, winrt::com_ptr, or equivalent, you can remove this
                m_initializer = nullptr;
            }
        }

        MidiDesktopAppSdkInitializer() = default;

        ~MidiDesktopAppSdkInitializer()
        {
            // in case it wasn't called earlier
            ShutdownSdkRuntime();
        }
    };
}

#endif