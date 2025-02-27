// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include "Microsoft.Windows.Devices.Midi2.Initialization.hpp"

#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Messages.h>

#include "winstring.h"
#include "roapi.h"

using namespace Microsoft::Windows::Devices::Midi2::Initialization;

bool MidiAppSdkInitializationTests::ClassSetup()
{
    winrt::init_apartment();

    return true;
}

bool MidiAppSdkInitializationTests::ClassCleanup()
{
    winrt::uninit_apartment();

    return true;
}


HRESULT CheckClassActivation(const winrt::hstring& activatableClassId)
{
    HSTRING clsid;
    IInspectable* instance;

    RETURN_IF_FAILED(WindowsCreateString(activatableClassId.c_str(), activatableClassId.size(), &clsid));
    RETURN_IF_FAILED(RoActivateInstance(clsid, &instance));

    RETURN_IF_FAILED(WindowsDeleteString(clsid));

    return S_OK;
}



HRESULT CheckClassRegistration(const winrt::hstring& activatableClassId, winrt::guid expectedClsid)
{
    HSTRING clsid;
    IInspectable* instance;

    RETURN_IF_FAILED(WindowsCreateString(activatableClassId.c_str(), activatableClassId.size(), &clsid));
    RETURN_IF_FAILED(RoActivateInstance(clsid, &instance));

    RETURN_IF_FAILED(WindowsDeleteString(clsid));

    ULONG iidCount;
    IID* iids;

    bool found{ false };

    RETURN_IF_FAILED(instance->GetIids(&iidCount, &iids));
    RETURN_HR_IF_NULL(E_UNEXPECTED, iids);

    for (ULONG i = 0; i < iidCount; i++)
    {
        winrt::guid checkGuid = iids[i];

        std::cout << winrt::to_string(winrt::to_hstring(checkGuid)) << std::endl;

        if (checkGuid == expectedClsid)
        {
            found = true;
            break;
        }
    }

    CoTaskMemFree(iids);

    //if (found)
    //{
    return S_OK;
    //}
    //else
    //{
    //    return E_NOINTERFACE;
    //}

}



void MidiAppSdkInitializationTests::TestInitialization()
{
    std::shared_ptr<MidiDesktopAppSdkInitializer> initializer;

    initializer = std::make_shared<MidiDesktopAppSdkInitializer>();

    VERIFY_IS_TRUE(initializer->InitializeSdkRuntime());
    VERIFY_IS_TRUE(initializer->EnsureServiceAvailable());

    //std::cout << "\nThis ref count should be 1" << std::endl;
    //std::cout << "Ref count before shutdown: " << initializer->TESTGetCurrentRefCount() << std::endl;
    initializer->ShutdownSdkRuntime();

    initializer.reset();

    // test the test
    VERIFY_FAILED(CheckClassActivation(L"Microsoft.Windows.Devices.Midi2.BogusClassNameThatDoesntExist"));

    // Verify we can't activate an SDK type. The SDK is unloaded
    // so the detours should be returned back to normal
    VERIFY_FAILED(CheckClassActivation(L"Microsoft.Windows.Devices.Midi2.MidiGroup"));
}

void MidiAppSdkInitializationTests::TestMultipleInitialization()
{
    // first init
    {
        MidiDesktopAppSdkInitializer initializer;

        VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
        VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

        // verify we can resolve the type and call code in the implementation
        std::cout << "Midi Time is: " << MidiClock::Now() << std::endl;

        // todo: check COM reference count
        //std::cout << "\nThis ref count should be 1" << std::endl;
        //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
        initializer.ShutdownSdkRuntime();
    }

    // we shouldn't be able to resolve this here
    VERIFY_FAILED(CheckClassActivation(L"Microsoft.Windows.Devices.Midi2.MidiGroup"));

    // second init
    {
        MidiDesktopAppSdkInitializer initializer;

        VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
        VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

        // verify we can resolve the type and call code in the implementation
        std::cout << "Midi Time is: " << MidiClock::Now() << std::endl;

        // todo: check COM reference count

        //std::cout << "\nThis ref count should be 1" << std::endl;
        //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
        initializer.ShutdownSdkRuntime();
    }

    // we shouldn't be able to resolve this here
    VERIFY_FAILED(CheckClassActivation(L"Microsoft.Windows.Devices.Midi2.MidiGroup"));

    // nested init
    {
        MidiDesktopAppSdkInitializer initializer;

        VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
        VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

        // verify we can resolve the type and call code in the implementation
        std::cout << "Outer Midi Time 1 is: " << MidiClock::Now() << std::endl;

        //std::cout << "\nThis ref count should be 1" << std::endl;
        //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;

        {
            MidiDesktopAppSdkInitializer initializer2;

            VERIFY_IS_TRUE(initializer2.InitializeSdkRuntime());
            VERIFY_IS_TRUE(initializer2.EnsureServiceAvailable());

            // verify we can resolve the type and call code in the implementation
            std::cout << "Nested Midi Time is: " << MidiClock::Now() << std::endl;
            std::cout << "'" << winrt::to_string(MidiChannel::LongLabelPlural()) << "' is the long plural version of 'Channel'" << std::endl;


            //std::cout << "\nThis ref count should be 2" << std::endl;
            //std::cout << "Ref count before shutdown: " << initializer2.TESTGetCurrentRefCount() << std::endl;

            {
                MidiDesktopAppSdkInitializer initializer3;

                VERIFY_IS_TRUE(initializer3.InitializeSdkRuntime());
                VERIFY_IS_TRUE(initializer3.EnsureServiceAvailable());

                //std::cout << "\nThis ref count should be 3" << std::endl;
                //std::cout << "Ref count before shutdown: " << initializer3.TESTGetCurrentRefCount() << std::endl;
                initializer3.ShutdownSdkRuntime();
            }

            {
                MidiDesktopAppSdkInitializer initializer3;

                VERIFY_IS_TRUE(initializer3.InitializeSdkRuntime());
                VERIFY_IS_TRUE(initializer3.EnsureServiceAvailable());

                //std::cout << "\nThis ref count should be 3" << std::endl;
                //std::cout << "Ref count before shutdown: " << initializer3.TESTGetCurrentRefCount() << std::endl;
                initializer3.ShutdownSdkRuntime();
            }

            {
                MidiDesktopAppSdkInitializer initializer3;

                VERIFY_IS_TRUE(initializer3.InitializeSdkRuntime());
                VERIFY_IS_TRUE(initializer3.EnsureServiceAvailable());

                //std::cout << "\nThis ref count should be 3" << std::endl;
                //std::cout << "Ref count before shutdown: " << initializer3.TESTGetCurrentRefCount() << std::endl;
                initializer3.ShutdownSdkRuntime();
            }


            //std::cout << "\nThis ref count should be 2" << std::endl;
            //std::cout << "Ref count before shutdown: " << initializer2.TESTGetCurrentRefCount() << std::endl;
            initializer2.ShutdownSdkRuntime();

            // this should return false because this is unloaded
            VERIFY_IS_FALSE(initializer2.CheckForMinimumRequiredSdkVersion(0, 0, 0));

        }

        // call into initializer after shutting down the second instance of it
        VERIFY_IS_TRUE(initializer.CheckForMinimumRequiredSdkVersion(0, 0, 0));
        VERIFY_IS_FALSE(initializer.CheckForMinimumRequiredSdkVersion(99, 0, 0));


        // verify we can still resolve the type and call code in the implementation
        std::cout << "Outer Midi Time 2 is: " << MidiClock::Now() << std::endl;
        // use a different type just in case there's caching
        std::cout << "'" << winrt::to_string(MidiGroup::LongLabelPlural()) << "' is the long plural version of 'Group'" << std::endl;

        // todo: check COM reference count

        //std::cout << "\nThis ref count should be 1" << std::endl;
        //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
        initializer.ShutdownSdkRuntime();

        // this should return false. If it doesn't, our ref counts are messed up
        VERIFY_IS_FALSE(initializer.CheckForMinimumRequiredSdkVersion(0, 0, 0));

    }

    // we shouldn't be able to resolve this here
    VERIFY_FAILED(CheckClassActivation(L"Microsoft.Windows.Devices.Midi2.MidiGroup"));

}

void MidiAppSdkInitializationTests::TestResolvingTypes()
{
    MidiDesktopAppSdkInitializer initializer;

    VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
    VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

    // TODO: test resolving every type






    //std::cout << "\nThis ref count should be 1" << std::endl;
    //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
    initializer.ShutdownSdkRuntime();
}



void MidiAppSdkInitializationTests::ValidateBackwardsCompatibilityWinMD()
{
    MidiDesktopAppSdkInitializer initializer;

    VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
    VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

    // TODO: validate that the clsids of each type have not changed

    // todo: use the xlang metadata parser to pull in our reference winmd and try to
    // resolve and activate the types in it.


    VERIFY_SUCCEEDED(CheckClassRegistration(L"Microsoft.Windows.Devices.Midi2.MidiChannel", winrt::guid()));
    //VERIFY_SUCCEEDED(CheckClassRegistration(L"Microsoft.Windows.Devices.Midi2.IMidiChannelStatics", winrt::guid()));
    VERIFY_SUCCEEDED(CheckClassRegistration(L"Microsoft.Windows.Devices.Midi2.MidiGroup", winrt::guid()));
    //VERIFY_SUCCEEDED(CheckClassRegistration(L"Microsoft.Windows.Devices.Midi2.IMidiGroupStatics", winrt::guid()));

    // not all types can be activated, because many use factory methods from other classes. Example: midisession, midiconnection
    //VERIFY_SUCCEEDED(CheckClassRegistration(L"Microsoft.Windows.Devices.Midi2.MidiSession", winrt::guid()));

    //std::cout << "\nThis ref count should be 1" << std::endl;
    //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
    initializer.ShutdownSdkRuntime();

}




