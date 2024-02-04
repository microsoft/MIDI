// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"


HRESULT
CMidiSessionTracker::Initialize()
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientSession(
    GUID SessionId,
    DWORD ClientProcessId,
    LPCWSTR ProcessName, 
    LPCWSTR SessionName
)
{
    MidiSessionEntry entry;

    entry.SessionId = SessionId;
    entry.ClientProcessId = ClientProcessId;
    entry.ProcessName = ProcessName;
    entry.SessionName = SessionName;

    GetSystemTime(&entry.StartTime);

    m_sessions[SessionId] = entry;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientSession(
    GUID SessionId
)
{
    if (m_sessions.find(SessionId) != m_sessions.end())
    {
        m_sessions.erase(SessionId);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientEndpointConnection(
    GUID SessionId, 
    LPCWSTR ConnectionEndpointInterfaceId
)
{
    MidiSessionConnectionEntry newConnection;

    newConnection.ConnectedEndpointInterfaceId = ConnectionEndpointInterfaceId;
    newConnection.InstanceCount = 1;
    GetSystemTime(&newConnection.EarliestStartTime);

    if (auto sessionEntry = m_sessions.find(SessionId); sessionEntry != m_sessions.end())
    {
        if (auto connection = sessionEntry->second.Connections.find(ConnectionEndpointInterfaceId); connection != sessionEntry->second.Connections.end())
        {
            // connection already exists. Increment it
            connection->second.InstanceCount++;
        }
        else
        {
            sessionEntry->second.Connections[ConnectionEndpointInterfaceId] = newConnection;
        }
    }
    else
    {
        // we have a connection and no session. Create a session to hold it and
        // then call this again
        RETURN_IF_FAILED(AddClientSession(SessionId, 0, L"Unknown", L"Unknown"));
        RETURN_IF_FAILED(AddClientEndpointConnection(SessionId, ConnectionEndpointInterfaceId));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientEndpointConnection(
    GUID SessionId, 
    LPCWSTR ConnectionEndpointInterfaceId
)
{
    if (auto sessionEntry = m_sessions.find(SessionId); sessionEntry != m_sessions.end())
    {
        if (auto connection = sessionEntry->second.Connections.find(ConnectionEndpointInterfaceId); connection != sessionEntry->second.Connections.end())
        {
            // connection already exists. Decrement it
            connection->second.InstanceCount--;

            if (connection->second.InstanceCount <= 0)
            {
                sessionEntry->second.Connections.erase(ConnectionEndpointInterfaceId);
            }
        }
    }

    return S_OK;

}




_Use_decl_annotations_
HRESULT 
CMidiSessionTracker::GetSessionListJson(
    BSTR* SessionList
)
{
    UNREFERENCED_PARAMETER(SessionList);

    json::JsonObject rootObject;
    json::JsonArray sessionsArray;

    for (auto sessionIter = m_sessions.begin(); sessionIter != m_sessions.end(); sessionIter++)
    {
        json::JsonObject sessionObject;

        internal::JsonSetGuidProperty(sessionObject, L"id", sessionIter->second.SessionId);
        internal::JsonSetWStringProperty(sessionObject, L"name", sessionIter->second.SessionName);
        internal::JsonSetWStringProperty(sessionObject, L"processName", sessionIter->second.ProcessName);
        internal::JsonSetDoubleProperty(sessionObject, L"clientProcessId", sessionIter->second.ClientProcessId);
        internal::JsonSetWStringProperty(sessionObject, L"startTime", internal::SystemTimeToDateTimeString(sessionIter->second.StartTime));

        // now add all the client connections

        json::JsonArray connectionsArray;

        for (auto connectionIter = sessionIter->second.Connections.begin(); connectionIter != sessionIter->second.Connections.end(); connectionIter++)
        {
            json::JsonObject connectionObject;

            internal::JsonSetWStringProperty(sessionObject, L"earliestStartTime", internal::SystemTimeToDateTimeString(connectionIter->second.EarliestStartTime));
            internal::JsonSetDoubleProperty(sessionObject, L"instanceCount", connectionIter->second.InstanceCount);
            internal::JsonSetWStringProperty(sessionObject, L"endpointId", connectionIter->second.ConnectedEndpointInterfaceId);

            connectionsArray.Append(connectionObject);
        }

        internal::JsonSetArrayProperty(sessionObject, L"connections", connectionsArray);


        sessionsArray.Append(sessionObject);
    }

    // should set up constants for these
    rootObject.SetNamedValue(L"sessions", sessionsArray);

    CComBSTR ccbstr;
    ccbstr.Empty();

    ccbstr = rootObject.Stringify().c_str();

    RETURN_IF_FAILED(ccbstr.CopyTo(SessionList));

    return S_OK;
}

HRESULT
CMidiSessionTracker::Cleanup()
{
    return S_OK;
}