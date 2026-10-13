// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The MIDI endpoint the service sees for one rtpMIDI connection. A client entry set to keep its
// endpoint has one that outlives its connections and is handed each new one, so the bidi talks to
// this rather than to a connection.
// ============================================================================

#pragma once

class RtpMidiConnection;

class RtpMidiEndpoint
{
public:
    RtpMidiEndpoint(
        _In_ GUID const& entryId,
        _In_ std::wstring const& instanceId,
        _In_ std::wstring const& endpointName);

    GUID EntryId() const noexcept { return m_entryId; }

    // as the device is created with it, and the form ids are compared and removed in
    std::wstring const& InstanceId() const noexcept { return m_instanceId; }
    std::wstring const& NormalizedInstanceId() const noexcept { return m_normalizedInstanceId; }

    std::wstring const& EndpointName() const noexcept { return m_endpointName; }

    // empty until the service has activated it
    void SetInterfaceId(_In_ std::wstring const& interfaceId);
    std::wstring InterfaceId() const;

    // A kept endpoint stays when its connection ends, and drops what is sent to it until the next one
    void SetKept(_In_ bool const kept) noexcept { m_kept = kept; }
    bool IsKept() const noexcept { return m_kept.load(); }

    void Attach(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool DetachIfCurrent(_In_ RtpMidiConnection const* connection);
    std::shared_ptr<RtpMidiConnection> Connection() const;

    // When it last lost its connection, or was made without one. 0 while it has one.
    uint64_t DisconnectedSinceTick() const;

    // the service's side, through the bidi
    HRESULT ConnectMidiCallback(_In_ IMidiCallback* callback, _In_ LONGLONG const context);
    HRESULT DisconnectMidiCallbackIfCurrent(_In_ IMidiCallback* callback);
    HRESULT SendToNetwork(_In_reads_bytes_(length) PVOID const data, _In_ UINT const length);

private:
    GUID const m_entryId{};
    std::wstring const m_instanceId;
    std::wstring const m_normalizedInstanceId;
    std::wstring const m_endpointName;
    std::atomic<bool> m_kept{ false };

    // Never held across a send. The connection's own locks are taken inside it, never the reverse.
    mutable std::mutex m_lock;
    std::wstring m_interfaceId;
    std::shared_ptr<RtpMidiConnection> m_connection;
    uint64_t m_disconnectedSinceTick{ 0 };
    wil::com_ptr_nothrow<IMidiCallback> m_callback;
    LONGLONG m_callbackContext{ 0 };
};
