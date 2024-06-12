// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

int __cdecl main()
{
    winrt::init_apartment();

    // this program does nothing. It's a convenience to gather all the generated header 
    // files for the C++ projection of the winrt SDKs. As such, this project must always
    // reference the entire set of SDK projects.

    return 0;
}
