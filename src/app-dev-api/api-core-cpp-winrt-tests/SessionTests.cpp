// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>
#include <Windows.h>

//using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;


TEST_CASE("Connected.Session.CreateSession Create new session")
{
    winrt::hstring sessionName = L"Test Session Name";

    auto settings = MidiSessionSettings::Default();
    auto session = MidiSession::CreateSession(sessionName, settings);

    REQUIRE((bool)(session != nullptr));

    REQUIRE((bool)(session.IsOpen()));

    REQUIRE((bool)(session.Name() == sessionName));

    REQUIRE((bool)(session.Id().empty() == false));

    REQUIRE((bool)(session.Connections().Size() == 0));

    session.Close();
}