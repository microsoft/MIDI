// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once



inline std::shared_ptr<init::MidiDesktopAppSdkInitializer> StartSDK()
{
    auto initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();

    VERIFY_IS_TRUE(initializer->InitializeSdkRuntime());
    VERIFY_IS_TRUE(initializer->EnsureServiceAvailable());

    return initializer;
}

inline std::shared_ptr<init::MidiDesktopAppSdkInitializer> InitWinRTAndSDK_MTA()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    return StartSDK();
}

inline std::shared_ptr<init::MidiDesktopAppSdkInitializer> InitWinRTAndSDK_STA()
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    return StartSDK();
}


inline void ShutdownSDKAndWinRT(std::shared_ptr<init::MidiDesktopAppSdkInitializer>& initializer)
{
    std::cout << "Shutting down SDK runtime" << std::endl;
    initializer->ShutdownSdkRuntime();

    std::cout << "Resetting initializer pointer" << std::endl;
    initializer.reset();

    std::cout << "Uninitializing apartment" << std::endl;
    winrt::uninit_apartment();
}