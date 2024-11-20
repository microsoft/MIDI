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
// we also use macros like SUCCEEDED


#define SAFE_COTASKMEMFREE(p) \
    if (NULL != p) { \
        CoTaskMemFree(p); \
        (p) = NULL; \
    }



// these are files generated from the MIDL compiler and are
// provided alongside this file for use in your apps
// they should be included in your app before this file. They
// are not included here because that can result in dups
//#include "WindowsMidiServicesClientInitialization.h"
//#include "WindowsMidiServicesClientInitialization_i.c"

namespace Microsoft::Windows::Devices::Midi2::Initialization
{
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
        // will change regularly. The aka will always point to a page where the latest SDK
        // runtime can be installed.
        const std::wstring LatestMidiAppSdkDownloadUrl{ L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller" };

        // guid for the initializer

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
        // call before calling this function. Do not call CoInitializeEx because
        // we're also using WinRT and the other two approaches are more appropriate
        bool InitializeSdkRuntime()
        {
            // check if already initialized
            if (m_initializer != nullptr) return false;

            if (SUCCEEDED(CoCreateInstance(
                    __uuidof(MidiClientInitializer),
                    NULL,
                    CLSCTX::CLSCTX_INPROC_SERVER | CLSCTX::CLSCTX_FROM_DEFAULT_CONTEXT,
                    IID_IMidiClientInitializer,
                    (LPVOID*)&m_initializer
                )))
            {
                if (m_initializer != nullptr)
                {
                    if (SUCCEEDED(m_initializer->Initialize()))
                    {
                        return true;
                    }
                    else
                    {
                        // SDK failed to initialize. Possible cause would be a corrupted
                        // install of some sort, this is being called from inside a packaged
                        // app (this bootstrapper is for desktop apps only) or perhaps there's 
                        // other code in this process which is redirecting WinRT type activation?
                        return false;
                    }
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
                return false;
            }
        }

        // If you know your application requires a minimum version of the SDK, because maybe you use features
        // added later, or you know there was an important bug fix, you can use this function, after initialization
        // to check for a minimum required version. Use the provided URL to download the latest runtime.
        // You will need to shutdown the SDK before replacing it.
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

                m_initializer->GetInstalledWindowsMidiServicesSdkVersion(
                    nullptr,                    // build platform
                    &installedVersionMajor,     // major
                    &installedVersionMinor,     // minor
                    &installedVersionRevision,  // revision
                    nullptr,                    // date number
                    nullptr,                    // time number
                    nullptr,                    // buildSource string. Remember to cotaskmemfree if provided
                    nullptr,                    // versionName string. Remember to cotaskmemfree if provided
                    nullptr                     // versionFullString string. Remember to cotaskmemfree if provided
                    );

                if (minRequiredVersionMajor > installedVersionMajor) return false;
                if (minRequiredVersionMinor > installedVersionMinor) return false;
                if (minRequiredVersionRevision > installedVersionRevision) return false;

                return true;
            }
            else
            {
                // the SDK has not been initialized, so we cannot get a version from it.
                // the caller should call InitializeSdkRuntime first
                return false;
            }
        }

        //// TODO: function to return the installed SDK version
        //bool GetSdkVersion(PWINDOWSMIDISERVICESAPPSDKVERSION installedVersion)
        //{
        //    if (m_initializer != nullptr)
        //    {
        //        if (SUCCEEDED(m_initializer->GetInstalledWindowsMidiServicesSdkVersion(installedVersion)))
        //        {
        //            return true;
        //        }
        //    }

        //    return false;
        //}



        void ShutdownSdkRuntime()
        {
            if (m_initializer != nullptr)
            {
                // release the COM component. When it shuts down, it will clean up
                // the activation hooks.

                m_initializer->Shutdown();  // if you are using wil error logging, this is a good place to use LOG_IF_FAILED(m_initializer->Shutdown())
                m_initializer->Release();   // if using wil::com_ptr_nothrow, you can remove this

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