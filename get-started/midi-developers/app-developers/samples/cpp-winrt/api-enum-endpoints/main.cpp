// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include "pch.h"
#include <iostream>

using namespace winrt::Windows::Devices::Midi2;        // API

// standard WinRT enumeration support. This is how you find attached devices.
using namespace winrt::Windows::Devices::Enumeration;

// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;


int main()
{
    winrt::init_apartment();


    std::cout << std::endl << "Enumerating endpoints..." << std::endl;


    


}
