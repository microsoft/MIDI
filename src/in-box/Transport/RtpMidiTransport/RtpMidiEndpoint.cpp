// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The MIDI endpoint the service sees for one rtpMIDI connection. No tracing on the message path.
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
RtpMidiEndpoint::RtpMidiEndpoint(
    GUID const& entryId,
    std::wstring const& instanceId,
    std::wstring const& endpointName,
    std::wstring const& hostRemoteName) :
    m_entryId(entryId),
    m_instanceId(instanceId),
    m_normalizedInstanceId(internal::NormalizeDeviceInstanceIdWStringCopy(instanceId)),
    m_endpointName(endpointName),
    m_hostRemoteName(hostRemoteName),
    m_disconnectedSinceTick(GetTickCount64())
{
}


_Use_decl_annotations_
void
RtpMidiEndpoint::SetInterfaceId(std::wstring const& interfaceId)
{
    auto lock = m_lock.lock_exclusive();

    m_interfaceId = interfaceId;

    if (m_connection != nullptr) m_connection->SetEndpointInterfaceId(m_interfaceId);
}

std::wstring
RtpMidiEndpoint::InterfaceId() const
{
    auto lock = m_lock.lock_shared();
    return m_interfaceId;
}


_Use_decl_annotations_
bool
RtpMidiEndpoint::Attach(std::shared_ptr<RtpMidiConnection> const& connection)
{
    if (connection == nullptr) return false;

    {
        auto lock = m_lock.lock_exclusive();

        // never in place of one still running
        if (connection->IsClosed()) return false;

        if (m_connection != nullptr && m_connection != connection)
        {
            if (m_callback != nullptr) LOG_IF_FAILED(m_connection->DisconnectMidiCallbackIfCurrent(m_callback.get()));
            m_connection->ClearEndpointInterfaceId();
        }

        m_connection = connection;
        m_disconnectedSinceTick = 0;
        m_releaseWhenConnectionEnds = false;

        m_connection->SetEndpointInterfaceId(m_interfaceId);

        // so an app which kept the endpoint open hears the remote from its first message
        if (m_callback != nullptr) LOG_IF_FAILED(m_connection->ConnectMidiCallback(m_callback.get(), m_callbackContext));
    }

    // A connection is closed before it is reported down. One that closed just now may already have
    // been reported, before it was attached here, so nothing else would ever take it off.
    if (connection->IsClosed())
    {
        DetachIfCurrent(connection.get());
        return false;
    }

    return true;
}

_Use_decl_annotations_
bool
RtpMidiEndpoint::DetachIfCurrent(RtpMidiConnection const* connection)
{
    auto lock = m_lock.lock_exclusive();

    if (connection == nullptr || m_connection.get() != connection) return false;

    if (m_callback != nullptr) LOG_IF_FAILED(m_connection->DisconnectMidiCallbackIfCurrent(m_callback.get()));
    m_connection->ClearEndpointInterfaceId();
    m_connection.reset();

    m_disconnectedSinceTick = GetTickCount64();

    return true;
}

std::shared_ptr<RtpMidiConnection>
RtpMidiEndpoint::Connection() const
{
    auto lock = m_lock.lock_shared();
    return m_connection;
}

uint64_t
RtpMidiEndpoint::DisconnectedSinceTick() const
{
    auto lock = m_lock.lock_shared();
    return m_disconnectedSinceTick;
}


_Use_decl_annotations_
HRESULT
RtpMidiEndpoint::ConnectMidiCallback(IMidiCallback* callback, LONGLONG const context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    auto lock = m_lock.lock_exclusive();

    m_callback = callback;
    m_callbackContext = context;

    if (m_connection != nullptr) RETURN_IF_FAILED(m_connection->ConnectMidiCallback(callback, context));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
RtpMidiEndpoint::DisconnectMidiCallbackIfCurrent(IMidiCallback* callback)
{
    auto lock = m_lock.lock_exclusive();

    // the service can open the endpoint again before the old bidi shuts down
    if (m_callback.get() != callback) return S_FALSE;

    if (m_connection != nullptr) LOG_IF_FAILED(m_connection->DisconnectMidiCallbackIfCurrent(callback));

    m_callback = nullptr;
    m_callbackContext = 0;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
RtpMidiEndpoint::SendToNetwork(PVOID const data, UINT const length)
{
    std::shared_ptr<RtpMidiConnection> connection{ nullptr };

    {
        auto lock = m_lock.lock_exclusive();
        connection = m_connection;
    }

    // An error here does not reach the app. The service holds the message and retries it, so the
    // app's queue fills and its sends stall. A remote that is away is a device switched off instead.
    if (connection == nullptr) return m_kept ? S_OK : HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED);

    auto const hr = connection->SendToNetwork(data, length);

    if (hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED) && m_kept) return S_OK;

    return hr;
}
