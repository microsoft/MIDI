// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "color.hpp"

// TEMP
#define VERIFY_IS_TRUE(c)   assert(c)
#define VERIFY_IS_FALSE(c)  assert(!(c))


int __cdecl main()
{
   // system("pause");


    for (auto const& threadingModel : { winrt::apartment_type::multi_threaded, winrt::apartment_type::single_threaded })
    {
        winrt::init_apartment(threadingModel);

        // first init
        {
            init::MidiDesktopAppSdkInitializer initializer;

            VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
            VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

            // verify we can resolve the type and call code in the implementation
            std::cout << "Midi Time is: " << midi2::MidiClock::Now() << std::endl;

            // check COM reference count
            //std::cout << "\nThis ref count should be 1" << std::endl;
            //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
            initializer.ShutdownSdkRuntime();
        }

        // second init
        {
            init::MidiDesktopAppSdkInitializer initializer;

            VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
            VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

            // verify we can resolve the type and call code in the implementation
            std::cout << "Midi Time is: " << midi2::MidiClock::Now() << std::endl;

            // todo: check COM reference count

            //std::cout << "\nThis ref count should be 1" << std::endl;
            //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
            initializer.ShutdownSdkRuntime();
        }


        // nested init
        {
            init::MidiDesktopAppSdkInitializer initializer;

            VERIFY_IS_TRUE(initializer.InitializeSdkRuntime());
            VERIFY_IS_TRUE(initializer.EnsureServiceAvailable());

            // verify we can resolve the type and call code in the implementation
            std::cout << "Outer Midi Time 1 is: " << midi2::MidiClock::Now() << std::endl;

            //std::cout << "\nThis ref count should be 1" << std::endl;
            //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;

            {
                init::MidiDesktopAppSdkInitializer initializer2;

                VERIFY_IS_TRUE(initializer2.InitializeSdkRuntime());
                VERIFY_IS_TRUE(initializer2.EnsureServiceAvailable());

                // verify we can resolve the type and call code in the implementation
                //std::cout << "Nested Midi Time is: " << midi2::MidiClock::Now() << std::endl;
                //std::cout << "'" << winrt::to_string(midi2::MidiChannel::LongLabelPlural()) << "' is the long plural version of 'Channel'" << std::endl;

                // todo: check COM reference count


                {
                    init::MidiDesktopAppSdkInitializer initializer3;

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
            std::cout << "Outer Midi Time 2 is: " << midi2::MidiClock::Now() << std::endl;
            // use a different type just in case there's caching
            std::cout << "'" << winrt::to_string(midi2::MidiGroup::LongLabelPlural()) << "' is the long plural version of 'Group'" << std::endl;

            // todo: check COM reference count

            //std::cout << "\nThis ref count should be 1" << std::endl;
            //std::cout << "Ref count before shutdown: " << initializer.TESTGetCurrentRefCount() << std::endl;
            initializer.ShutdownSdkRuntime();

            // this should return false. If it doesn't, our ref counts are messed up
            VERIFY_IS_FALSE(initializer.CheckForMinimumRequiredSdkVersion(0, 0, 0));

        }

        winrt::uninit_apartment();

    }


}
