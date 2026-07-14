// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


bool MidiReportingTests::ClassSetup()
{
    StartWinRTMTA();
    return true;
}

bool MidiReportingTests::ClassCleanup()
{
    ShutdownWinRT();
    return true;
}


void MidiReportingTests::TestEnumerateTransports()
{
    auto transports = MidiReporting::GetInstalledTransportPlugins();

    VERIFY_IS_NOT_NULL(transports);
    VERIFY_IS_TRUE(transports.Size() > 0);

    for (auto const& transport : transports)
    {
        std::wcout << L"Transport: " << transport.Name().c_str() << std::endl;

        VERIFY_IS_FALSE(transport.Name().empty());
    }
}


MidiServiceSessionInfo FindSession(collections::IVector<MidiServiceSessionInfo> sessions, winrt::guid sessionId)
{
    for (auto const& session : sessions)
    {
        if (session.SessionId() == sessionId)
        {
            return session;
        }
    }
    return nullptr;
}


void MidiReportingTests::TestEnumerateSessions()
{
    // create a session 

    winrt::hstring thisSessionName = L"TAEFTest Session";

    auto thisSession = MidiSession::Create(thisSessionName);
    VERIFY_IS_NOT_NULL(thisSession);

    // Test 1: check that session shows up in results, with no connections

    collections::IVector<MidiServiceSessionInfo> sessionList;

    sessionList = MidiReporting::GetActiveSessions();
    VERIFY_IS_NOT_NULL(sessionList);

    auto foundSession = FindSession(sessionList, thisSession.SessionId());
    VERIFY_IS_NOT_NULL(foundSession);
    VERIFY_IS_TRUE(foundSession.SessionName() == thisSessionName);
    VERIFY_IS_TRUE(foundSession.Connections().Size() == 0);


    // Test 2: open a connection and verify that the connection shows up

    // get a list of endpoints we can connect to
    auto endpoints = MidiEndpointDeviceInformation::FindAll();
    VERIFY_IS_NOT_NULL(endpoints);
    VERIFY_IS_TRUE(endpoints.Size() > 0);


    for (auto const& endpoint : endpoints)
    {
        // open the connection
        std::wcout << L"Testing connection to endpoint: " << endpoint.Name().c_str() << std::endl;
        auto conn = thisSession.CreateEndpointConnection(endpoint.EndpointDeviceId());
        VERIFY_IS_NOT_NULL(conn);
        VERIFY_IS_TRUE(conn.Open());

        // verify the connection shows up in the session info
        sessionList = MidiReporting::GetActiveSessions();
        foundSession = FindSession(sessionList, thisSession.SessionId());
        VERIFY_IS_NOT_NULL(foundSession);
        VERIFY_IS_TRUE(foundSession.Connections().Size() == 1);
        VERIFY_IS_TRUE(foundSession.Connections().GetAt(0).EndpointDeviceId() == endpoint.EndpointDeviceId());

        // close the connection
        thisSession.DisconnectEndpointConnection(conn.ConnectionId());

        // verify the connection is removed from the session info
        sessionList = MidiReporting::GetActiveSessions();
        foundSession = FindSession(sessionList, thisSession.SessionId());
        VERIFY_IS_NOT_NULL(foundSession);
        VERIFY_IS_TRUE(foundSession.Connections().Size() == 0);
    }

    std::wcout << L"Testing closing the session" << std::endl;
    thisSession.Close();

    sessionList = MidiReporting::GetActiveSessions();
    foundSession = FindSession(sessionList, thisSession.SessionId());
    VERIFY_IS_NULL(foundSession);

}
