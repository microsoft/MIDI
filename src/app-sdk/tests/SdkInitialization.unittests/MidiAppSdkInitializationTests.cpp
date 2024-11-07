// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include "MidiDesktopAppSdkBootstrapper.hpp"


void MidiAppSdkInitializationTests::TestInitialization()
{
    // check to see if SDK is installed. Obviously, for this test to work, it has to 
    // be run on a system with a valid SDK installation

    //VERIFY_IS_TRUE(MidiServicesInitializer::IsCompatibleDesktopAppSdkRuntimeInstalled());

    //VERIFY_IS_TRUE(MidiServicesInitializer::InitializeDesktopAppSdkRuntime());
    //VERIFY_IS_TRUE(MidiServicesInitializer::ShutdownDesktopAppSdkRuntime());

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
