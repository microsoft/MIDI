// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"


_Use_decl_annotations_
HRESULT
CMidiSessionTracker::Initialize(std::shared_ptr<CMidiClientManager>& clientManager)
{
    m_clientManager = clientManager;

    return S_OK;
}





HRESULT
CMidiSessionTracker::VerifyConnectivity()
{
    // if this gets called, we have connectivity
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientSessionInternal(
    GUID SessionId,
    LPCWSTR SessionName,
    DWORD ClientProcessId,
    LPCWSTR ClientProcessName,
    PVOID* ContextHandle
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(SessionId),
        TraceLoggingWideString(SessionName),
        TraceLoggingWideString(ClientProcessName)
        );

    MidiSessionEntry entry;

    entry.SessionId = SessionId;

    if (SessionName != nullptr)
    {
        entry.SessionName = internal::TrimmedWStringCopy(SessionName);
    }

    entry.ClientProcessId = ClientProcessId;

    if (ClientProcessName != nullptr)
    {
        entry.ProcessName = ClientProcessName;
    }

    entry.StartTime = std::chrono::system_clock::now();

//    GetSystemTime(&entry.StartTime);

    m_sessions[SessionId] = entry;

    // this can be anything that hangs around.
    // we should provide something more durable
    // here in case we ever use the pointer. Map
    // pointers are not guaranteed to be immutable
    // as I recall

    // TODO: We could make this number just a hash or something

    if (ContextHandle != nullptr)
    {
        auto contextHandleValue = &(m_sessions[SessionId]);

        *ContextHandle = (PVOID)contextHandleValue;

        m_sessionContextHandles[*ContextHandle] = SessionId;
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientSession(
    GUID SessionId,
    LPCWSTR SessionName
)
{
    UNREFERENCED_PARAMETER(SessionId);
    UNREFERENCED_PARAMETER(SessionName);

    // we don't implement this here. It's required for the abstraction.
    return E_NOTIMPL;
}


// TODO: I don't like how this can be called from any process
// So really should have PID verification, some shared key, or
// other security to tie these calls and their calling context
// to the specific app. 
_Use_decl_annotations_
HRESULT
CMidiSessionTracker::UpdateClientSessionName(
    GUID SessionId,
    LPCWSTR SessionName,
    DWORD ClientProcessId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(SessionId),
        TraceLoggingWideString(SessionName)
    );

    // clean up the session name
    std::wstring newName{ SessionName };
    internal::InPlaceTrim(newName);

    if (!newName.empty())
    {
        if (auto session = m_sessions.find(SessionId); session != m_sessions.end())
        {
            if (session->second.ClientProcessId == ClientProcessId)
            {
                m_sessions[SessionId].SessionName = newName;

                return S_OK;
            }
            else
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"E_NOTFOUND. No matching session for this client process.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingGuid(SessionId),
                    TraceLoggingWideString(SessionName)
                    );

                return E_NOTFOUND;
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"E_NOTFOUND. No matching session", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingGuid(SessionId),
                TraceLoggingWideString(SessionName)
            );

            return E_NOTFOUND;
        }
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"E_INVALIDARG. Trimmed session name is empty", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(SessionId),
            TraceLoggingWideString(SessionName)
        );

        return E_INVALIDARG;
    }

}


_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientSession(
    GUID SessionId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(SessionId)
    );

    if (m_sessions.find(SessionId) != m_sessions.end())
    {
        m_sessions.erase(SessionId);
    }

    return S_OK;
}


// this gets called when the client crashes
_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientSessionInternal(
    PVOID ContextHandle
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(ContextHandle)
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, ContextHandle);

    std::shared_ptr<CMidiClientManager> clientManager = m_clientManager.lock();

    auto sessionIdMapEntry = m_sessionContextHandles.find(ContextHandle);

    if (sessionIdMapEntry != m_sessionContextHandles.end())
    {
        auto sessionEntry = m_sessions.find(sessionIdMapEntry->second);

        if (sessionEntry != m_sessions.end())
        {
            // TODO: Remove each client connection using the MidiClientManager

            while (sessionEntry->second.ClientHandles.size() > 0)
            {
                auto clientHandle = *(sessionEntry->second.ClientHandles.begin());

                sessionEntry->second.ClientHandles.erase(sessionEntry->second.ClientHandles.begin());

                clientManager->DestroyMidiClient((handle_t)nullptr, clientHandle);
            }

            // Remove this session entry
            m_sessions.erase(sessionEntry);
        }
    }

    return S_OK;
}





_Use_decl_annotations_
HRESULT
CMidiSessionTracker::IsValidSession(
    GUID SessionId, 
    DWORD ClientProcessId
)
{

    if (auto session = m_sessions.find(SessionId); session != m_sessions.end())
    {
        if (session->second.ClientProcessId == ClientProcessId)
        {
            return S_OK;
        }
    }

    return E_NOTFOUND;
}



_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientEndpointConnection(
    GUID SessionId, 
    LPCWSTR ConnectionEndpointInterfaceId,
    MidiClientHandle ClientHandle
    )
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(SessionId),
        TraceLoggingWideString(ConnectionEndpointInterfaceId)
    );

    MidiSessionConnectionEntry newConnection;

    auto cleanEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(ConnectionEndpointInterfaceId);

    newConnection.ConnectedEndpointInterfaceId = cleanEndpointId;
    newConnection.InstanceCount = 1;
    newConnection.EarliestStartTime = std::chrono::system_clock::now();
       
    if (auto sessionEntry = m_sessions.find(SessionId); sessionEntry != m_sessions.end())
    {
        if (auto connection = sessionEntry->second.Connections.find(cleanEndpointId); connection != sessionEntry->second.Connections.end())
        {
            // connection already exists. Increment it
            connection->second.InstanceCount++;
        }
        else
        {
            sessionEntry->second.Connections[cleanEndpointId] = newConnection;
        }

        // need to end up removing these laters
        sessionEntry->second.ClientHandles.push_back(ClientHandle);
    }
    else
    {
        // we have a connection and no session. Fail

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingGuid(SessionId),
            TraceLoggingWideString(ConnectionEndpointInterfaceId),
            TraceLoggingWideString(L"No valid session found. Returning E_NOTFOUND.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return E_NOTFOUND;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientEndpointConnection(
    GUID SessionId, 
    LPCWSTR ConnectionEndpointInterfaceId,
    MidiClientHandle /*ClientHandle*/
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(SessionId),
        TraceLoggingWideString(ConnectionEndpointInterfaceId)
    );

    auto cleanEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(ConnectionEndpointInterfaceId);

    if (auto sessionEntry = m_sessions.find(SessionId); sessionEntry != m_sessions.end())
    {
        if (auto connection = sessionEntry->second.Connections.find(cleanEndpointId); connection != sessionEntry->second.Connections.end())
        {
            // connection already exists. Decrement it
            connection->second.InstanceCount--;

            if (connection->second.InstanceCount <= 0)
            {
                sessionEntry->second.Connections.erase(cleanEndpointId);
            }
        }
    }

    return S_OK;

}


_Use_decl_annotations_
HRESULT
CMidiSessionTracker::GetSessionList(
    LPSAFEARRAY* SessionDetailsList
)
{
    // TODO
    SessionDetailsList = nullptr;

    return S_OK;

}



_Use_decl_annotations_
HRESULT 
CMidiSessionTracker::GetSessionListJson(
    BSTR* SessionList
)
{
    json::JsonObject rootObject;
    json::JsonArray sessionsArray{};


    for (auto sessionIter = m_sessions.begin(); sessionIter != m_sessions.end(); sessionIter++)
    {
        json::JsonObject sessionObject;

        internal::JsonSetGuidProperty(sessionObject, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY, sessionIter->second.SessionId);
        sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY, json::JsonValue::CreateStringValue(sessionIter->second.SessionName));
        sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY, json::JsonValue::CreateStringValue(sessionIter->second.ProcessName));
        sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY, json::JsonValue::CreateStringValue(std::to_wstring(sessionIter->second.ClientProcessId)));
        internal::JsonSetDateTimeProperty(sessionObject, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, sessionIter->second.StartTime);

        // now add all the client connections

        json::JsonArray connectionsArray{};

        for (auto connectionIter = sessionIter->second.Connections.begin(); connectionIter != sessionIter->second.Connections.end(); connectionIter++)
        {
            json::JsonObject connectionObject;

            internal::JsonSetDateTimeProperty(sessionObject, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY, connectionIter->second.EarliestStartTime);
            connectionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY, json::JsonValue::CreateNumberValue(connectionIter->second.InstanceCount));
            connectionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY, json::JsonValue::CreateStringValue(connectionIter->second.ConnectedEndpointInterfaceId));
            
            connectionsArray.Append(connectionObject);
        }

        // append the connections array only if we have some
        if (connectionsArray.Size() > 0)
        {
            sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY, connectionsArray);
        }

        sessionsArray.Append(sessionObject);
    }

    rootObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY, sessionsArray);

    CComBSTR ccbstr;
    ccbstr.Empty();

    // we do this conversion step so we can trace log it
    std::wstring wstr = rootObject.Stringify().c_str();

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(wstr.c_str())
    );

    ccbstr = wstr.c_str();

    RETURN_IF_FAILED(ccbstr.CopyTo(SessionList));

    return S_OK;
}

HRESULT
CMidiSessionTracker::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_clientManager.reset();

    return S_OK;
}