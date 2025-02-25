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

using namespace Microsoft::Windows::Devices::Midi2::Initialization;

void MidiAppSdkInitializationTests::TestInitialization()
{
    winrt::init_apartment();


    MidiDesktopAppSdkInitializer initializer;

    VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());

    VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

    initializer.ShutdownSdkRuntime();

    // todo: check COM reference count


}

void MidiAppSdkInitializationTests::TestMultipleInitialization()
{
    winrt::init_apartment();

    // first init
    {
        MidiDesktopAppSdkInitializer initializer;

        VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
        VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

        // verify we can resolve the type and call code in the implementation
        std::cout << "Midi Time is: " << MidiClock::Now() << std::endl;

        // todo: check COM reference count

        initializer.ShutdownSdkRuntime();
    }

    // second init
    {
        MidiDesktopAppSdkInitializer initializer;

        VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
        VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

        // verify we can resolve the type and call code in the implementation
        std::cout << "Midi Time is: " << MidiClock::Now() << std::endl;

        // todo: check COM reference count

        initializer.ShutdownSdkRuntime();
    }


    // nested init
    {
        MidiDesktopAppSdkInitializer initializer;

        VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
        VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

        // verify we can resolve the type and call code in the implementation
        std::cout << "Outer Midi Time 1 is: " << MidiClock::Now() << std::endl;

        {
            MidiDesktopAppSdkInitializer initializer2;

            VERIFY_IS_TRUE(initializer2.InitializeSdkRuntime());
            VERIFY_IS_TRUE(initializer2.EnsureServiceAvailable());

            // verify we can resolve the type and call code in the implementation
            std::cout << "Nested Midi Time is: " << MidiClock::Now() << std::endl;
            std::cout << "'" << winrt::to_string(MidiChannel::LongLabelPlural()) << "' is the long plural version of 'Channel'" << std::endl;

            // todo: check COM reference count

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

        initializer.ShutdownSdkRuntime();

        // this should return false. If it doesn't, our ref counts are messed up
        VERIFY_IS_FALSE(initializer.CheckForMinimumRequiredSdkVersion(0, 0, 0));

    }

    // the SDK won't actually unload until the process unloads it

    winrt::uninit_apartment();
}

void MidiAppSdkInitializationTests::TestResolvingTypes()
{
    //VERIFY_IS_TRUE(MidiServicesInitializer::InitializeDesktopAppSdkRuntime());

    //auto cleanup = wil::scope_exit([&]
    //    {
    //        VERIFY_IS_TRUE(MidiServicesInitializer::ShutdownDesktopAppSdkRuntime());
    //    });

    //// try to create the types

    //std::wcout << L"MidiClock::Now() " << MidiClock::Now() << std::endl;

    
}






