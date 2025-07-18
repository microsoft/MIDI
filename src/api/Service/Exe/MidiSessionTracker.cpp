// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include <filesystem>


_Use_decl_annotations_
std::vector<MidiSessionEntry>::iterator
CMidiSessionTracker::FindSession(GUID sessionId, DWORD clientProcessId)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId, "session id"),
        TraceLoggingUInt32(clientProcessId, "client process"),
        TraceLoggingUInt32((UINT32)m_sessions.size(), "Total session count")
        );

    auto match = std::find_if(m_sessions.begin(), m_sessions.end(),
        [&sessionId, &clientProcessId](const MidiSessionEntry& entry)
        {
            //return
            //    entry.ClientProcessId == clientProcessId &&
            //    (InlineIsEqualGUID(entry.SessionId, sessionId) != 0);
            return
                entry.ClientProcessId == clientProcessId &&
                entry.SessionId == sessionId;
        });

    return match;
}

_Use_decl_annotations_
std::vector<MidiSessionEntry>::iterator
CMidiSessionTracker::FindSessionForContextHandle(PVOID contextHandle)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    if (contextHandle == nullptr) return m_sessions.end();

    auto match = std::find_if(m_sessions.begin(), m_sessions.end(),
        [&contextHandle](const MidiSessionEntry& val)
        {
            return val.ContextHandle == contextHandle;
        });

    return match;
}




_Use_decl_annotations_
HRESULT
CMidiSessionTracker::Initialize(std::shared_ptr<CMidiClientManager>& clientManager)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_clientManager = clientManager;

    return S_OK;
}

BOOL
CMidiSessionTracker::VerifyConnectivity()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    // if this gets called, we have connectivity
    return TRUE;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientSession(
    GUID sessionId,
    LPCWSTR sessionName,
    DWORD clientProcessId,
    wil::unique_handle& clientProcessHandle,
    PVOID* contextHandle
)
{

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId),
        TraceLoggingWideString(sessionName)
    );

    // context handle can be null for protocol manager
    //RETURN_HR_IF_NULL(E_INVALIDARG, contextHandle);


    auto lock = m_sessionsLock.lock();
    auto result = FindSession(sessionId, clientProcessId);

    if (result != m_sessions.end())
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Session already exists. Unable to add new.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_INVALIDARG);
    }


    std::wstring clientProcessName{};
    std::wstring modulePath{ 0 };
    modulePath.resize(MAX_PATH + 1);

    auto numPathCharacters = GetModuleFileNameEx(clientProcessHandle.get(), NULL, modulePath.data(), (DWORD)modulePath.capacity());
    if (numPathCharacters > 0)
    {
        clientProcessName = (std::filesystem::path(modulePath).filename()).c_str();
    }
    else
    {
        // couldn't get the current process name
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to get current process name", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Creating new session entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId),
        TraceLoggingWideString(sessionName),
        TraceLoggingWideString(clientProcessName.c_str())
    );


    MidiSessionEntry entry{ };

    entry.SessionId = sessionId;
    entry.ClientProcessId = clientProcessId;
    entry.StartTime = std::chrono::system_clock::now();

    if (sessionName != nullptr)
    {
        entry.SessionName = internal::TrimmedWStringCopy(sessionName);
    }
    else
    {
        entry.SessionName = L"Default";
    }

    if (!clientProcessName.empty())
    {
        entry.ProcessName = clientProcessName;
    }

    m_sessions.push_back(entry);

    if (contextHandle != nullptr)
    {
        // we use the pointer to the session entry as the context handle, so
        // we need to add it to the vector before we have that value

        auto savedEntryIterator = FindSession(sessionId, clientProcessId);
        auto contextHandleValue = &(*(savedEntryIterator));

        *contextHandle = (PVOID)contextHandleValue;

        savedEntryIterator->ContextHandle = contextHandleValue;
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Session created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId),
        TraceLoggingWideString(sessionName),
        TraceLoggingWideString(clientProcessName.c_str())
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::UpdateClientSessionName(
    GUID sessionId,
    LPCWSTR sessionName,
    DWORD clientProcessId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId),
        TraceLoggingWideString(sessionName)
    );

    // clean up the session name
    std::wstring newName{ sessionName };
    internal::InPlaceTrim(newName);

    if (!newName.empty())
    {
        auto lock = m_sessionsLock.lock();

        auto sess = FindSession(sessionId, clientProcessId);
        if (sess != m_sessions.end())
        {
            sess->SessionName = newName;
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
                TraceLoggingGuid(sessionId),
                TraceLoggingUInt32(clientProcessId),
                TraceLoggingWideString(sessionName)
            );

            RETURN_IF_FAILED(E_NOTFOUND);
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
            TraceLoggingGuid(sessionId),
            TraceLoggingUInt32(clientProcessId),
            TraceLoggingWideString(sessionName)
        );

        RETURN_IF_FAILED(E_INVALIDARG);
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientSession(
    GUID sessionId,
    DWORD clientProcessId
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId)
    );

    auto lock = m_sessionsLock.lock();
    
    auto session = FindSession(sessionId, clientProcessId);
    if (session != m_sessions.end())
    {
        m_sessions.erase(session);
    }
    else
    {
        RETURN_IF_FAILED(E_INVALIDARG);
    }

    return S_OK;
}


// this gets called when the client crashes
_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientSessionInternal(
    PVOID contextHandle
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(contextHandle, "context handle")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, contextHandle);

    auto lock = m_sessionsLock.lock();

    std::shared_ptr<CMidiClientManager> clientManager = m_clientManager.lock();

    if (auto sessionEntry = FindSessionForContextHandle(contextHandle); sessionEntry != m_sessions.end())
    {
        // Remove each client connection using the MidiClientManager

        while (sessionEntry->ClientHandles.size() > 0)
        {
            auto clientHandle = *(sessionEntry->ClientHandles.begin());

            sessionEntry->ClientHandles.erase(sessionEntry->ClientHandles.begin());

            LOG_IF_FAILED(clientManager->DestroyMidiClient(clientHandle));
        }

        // Remove this session entry
        m_sessions.erase(sessionEntry);
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(contextHandle, "context handle")
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::IsValidSession(
    GUID sessionId, 
    DWORD clientProcessId
)
{
    auto lock = m_sessionsLock.lock();
    auto session = FindSession(sessionId, clientProcessId);

    if (session != m_sessions.end())
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Session Valid. Found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(sessionId, "Session ID"),
            TraceLoggingUInt32(clientProcessId, "Client Process ID")
        );

        return S_OK;
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Session Invalid. NOT found in the tracker", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(sessionId, "Session ID"),
        TraceLoggingUInt32(clientProcessId, "Client Process ID"),
        TraceLoggingUInt32(static_cast<uint32_t>(m_sessions.size()), "Total session count")
    );

    // session is not present in the list, this is an expected
    // error for some situations
    RETURN_IF_FAILED(E_NOTFOUND);
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::AddClientEndpointConnection(
    GUID sessionId, 
    DWORD clientProcessId,
    LPCWSTR connectionEndpointInterfaceId,
    MidiClientHandle clientHandle
    )
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId),
        TraceLoggingWideString(connectionEndpointInterfaceId)
    );

    MidiSessionConnectionEntry newConnection;

    auto cleanEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(connectionEndpointInterfaceId);

    newConnection.ConnectedEndpointInterfaceId = cleanEndpointId;
    newConnection.InstanceCount = 1;
    newConnection.EarliestStartTime = std::chrono::system_clock::now();
       
    auto lock = m_sessionsLock.lock();

    auto sessionEntry = FindSession(sessionId, clientProcessId);
    if (sessionEntry != m_sessions.end())
    {
        if (auto connection = sessionEntry->Connections.find(cleanEndpointId); connection != sessionEntry->Connections.end())
        {
            // connection already exists. Increment it
            connection->second.InstanceCount++;
        }
        else
        {
            sessionEntry->Connections[cleanEndpointId] = newConnection;
        }

        // need to end up removing these laters
        sessionEntry->ClientHandles.push_back(clientHandle);
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
            TraceLoggingGuid(sessionId),
            TraceLoggingUInt32(clientProcessId),
            TraceLoggingWideString(connectionEndpointInterfaceId),
            TraceLoggingWideString(L"No valid session found. Returning E_NOTFOUND.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        RETURN_IF_FAILED(E_NOTFOUND);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiSessionTracker::RemoveClientEndpointConnection(
    GUID sessionId, 
    DWORD clientProcessId,
    LPCWSTR connectionEndpointInterfaceId,
    MidiClientHandle /*clientHandle*/
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId),
        TraceLoggingWideString(connectionEndpointInterfaceId)
    );

    auto lock = m_sessionsLock.lock();

    auto cleanEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(connectionEndpointInterfaceId);

    if (auto sessionEntry = FindSession(sessionId, clientProcessId); sessionEntry != m_sessions.end())
    {
        if (auto connection = sessionEntry->Connections.find(cleanEndpointId); connection != sessionEntry->Connections.end())
        {
            // connection already exists. Decrement it
            connection->second.InstanceCount--;

            if (connection->second.InstanceCount <= 0)
            {
                sessionEntry->Connections.erase(cleanEndpointId);
            }
        }
        else
        {
            RETURN_IF_FAILED(E_NOTFOUND);
        }
    }

    return S_OK;

}


_Use_decl_annotations_
HRESULT 
CMidiSessionTracker::GetSessionList(
    LPWSTR* sessionList
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto lock = m_sessionsLock.lock();

    json::JsonObject rootObject;
    json::JsonArray sessionsArray{};


    for (auto sessionIter = m_sessions.begin(); sessionIter != m_sessions.end(); sessionIter++)
    {
        json::JsonObject sessionObject;

        internal::JsonSetGuidProperty(sessionObject, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY, sessionIter->SessionId);
        sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY, json::JsonValue::CreateStringValue(sessionIter->SessionName));
        sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY, json::JsonValue::CreateStringValue(sessionIter->ProcessName));
        sessionObject.SetNamedValue(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY, json::JsonValue::CreateStringValue(std::to_wstring(sessionIter->ClientProcessId)));
        internal::JsonSetDateTimeProperty(sessionObject, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, sessionIter->StartTime);

        // now add all the client connections

        json::JsonArray connectionsArray{};

        for (auto connectionIter = sessionIter->Connections.begin(); connectionIter != sessionIter->Connections.end(); connectionIter++)
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

    //CComBSTR ccbstr;
    //ccbstr.Empty();

    //// we do this conversion step so we can trace log it
    //std::wstring wstr = rootObject.Stringify().c_str();

    //ccbstr = wstr.c_str();

    //RETURN_IF_FAILED(ccbstr.CopyTo(sessionList));

    if (internal::JsonStringifyObjectToOutParam(rootObject, sessionList))
    {
        return S_OK;
    }

    return E_FAIL;
}

HRESULT
CMidiSessionTracker::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_clientManager.reset();

    // Clean up sessions
    m_sessions.clear();


    return S_OK;
}