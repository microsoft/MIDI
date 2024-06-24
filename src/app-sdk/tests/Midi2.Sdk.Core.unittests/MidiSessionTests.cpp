// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"


void MidiSessionTests::TestCreateNewSession()
{
    winrt::hstring sessionName = L"Test Session Name";

    auto session = MidiSession::Create(sessionName);

    VERIFY_IS_NOT_NULL(session);

    VERIFY_IS_TRUE(session.IsOpen());

    VERIFY_ARE_EQUAL(session.Name(),sessionName);

    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    session.Close();
}

void MidiSessionTests::TestSessionList()
{
    winrt::hstring session1Name = L"Session 1 Name";
    winrt::hstring session2Name = L"Session 2 Name";

    auto session1 = MidiSession::Create(session1Name);
    auto session2 = MidiSession::Create(session2Name);

    VERIFY_IS_NOT_NULL(session1);
    VERIFY_IS_NOT_NULL(session2);

    auto sessionList = MidiReporting::GetActiveSessions();

    bool found1 = false;
    bool found2 = false;
    for (auto const& detail : sessionList)
    {
        if (detail.SessionId() == session1.SessionId())
        {
            found1 = true;
        }
        else if (detail.SessionId() == session2.SessionId())
        {
            found2 = true;
        }
    }
    VERIFY_IS_TRUE(found1);
    VERIFY_IS_TRUE(found2);

    session1.Close();
    session2.Close();
}


void MidiSessionTests::TestUpdateSessionName()
{
    winrt::hstring oldSessionName = L"Test Session Name";
    winrt::hstring newSessionName = L"New Session Name";

    auto session = MidiSession::Create(oldSessionName);

    VERIFY_IS_NOT_NULL(session);

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Name(), oldSessionName);

    // now update the name

    VERIFY_IS_TRUE(session.UpdateName(newSessionName));
    VERIFY_ARE_EQUAL(session.Name(), newSessionName);

    // To be thorough, we call the service to get a list of sessions and verify the name is updated there

    auto sessionList = MidiReporting::GetActiveSessions();

    bool found = false;
    for (auto const& detail : sessionList)
    {
        if (detail.SessionId() == session.SessionId())
        {
            VERIFY_IS_TRUE(detail.SessionName() == newSessionName);
            found = true;
            break;
        }
    }
    VERIFY_IS_TRUE(found);

    session.Close();
}
