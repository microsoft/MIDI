// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once



inline void StartWinRTMTA()
{
    std::cout << "Initializing apartment (MTA)" << std::endl;
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

inline void StartWinRTSTA()
{
    std::cout << "Initializing apartment (STA)" << std::endl;
    winrt::init_apartment(winrt::apartment_type::single_threaded);
}


inline void ShutdownWinRT()
{
 
    std::cout << "Uninitializing apartment" << std::endl;
    winrt::uninit_apartment();
}



inline bool HStringsAreCaseInsensitiveEqual(const winrt::hstring& lhs, const winrt::hstring& rhs)
{
    return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
}

