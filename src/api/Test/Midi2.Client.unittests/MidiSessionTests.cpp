// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"


using namespace winrt::Microsoft::Devices::Midi2;

void MidiSessionTests::TestCreateNewSessionWithoutSettings()
{
    winrt::hstring sessionName = L"Test Session Name";

    auto session = MidiSession::CreateSession(sessionName);

    VERIFY_IS_NOT_NULL(session);

    VERIFY_IS_TRUE(session.IsOpen());

    VERIFY_ARE_EQUAL(session.Name(),sessionName);

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    session.Close();
}


void MidiSessionTests::TestCreateNewSessionWithSettings()
{
    winrt::hstring sessionName = L"Test Session Name";
    MidiSessionSettings settings;
    settings.UseMmcssThreads(true);


    auto session = MidiSession::CreateSession(sessionName, settings);

    VERIFY_IS_NOT_NULL(session);

    VERIFY_IS_TRUE(session.IsOpen());

    VERIFY_ARE_EQUAL(session.Name(), sessionName);

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    session.Close();
}