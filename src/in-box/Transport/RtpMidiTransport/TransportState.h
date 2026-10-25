// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Transport-wide state shared by the endpoint and configuration managers.
// ============================================================================

#pragma once

class TransportState
{
public:
    static TransportState& Current();

    TransportState(_In_ TransportState const&) = delete;
    TransportState& operator=(_In_ TransportState const&) = delete;

    wil::com_ptr<CMidi2RtpMidiEndpointManager> GetEndpointManager();
    wil::com_ptr<CMidi2RtpMidiConfigurationManager> GetConfigurationManager();

    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();

    // Drops the references to both managers, and lets the approvals' deferred notification finish.
    // This class is a function-local static, so anything still held here is released at DLL unload,
    // after the service has destroyed the objects the managers hold. Called from both managers'
    // Shutdown, so it is safe to call twice. A manager calling this must hold its own reference for
    // the rest of its Shutdown.
    void Shutdown() noexcept;

    // The configuration file and the endpoint manager come up in an order this transport does not
    // control, so definitions are kept here and the endpoint manager acts on them when it can.
    void SetHostDefinition(_In_ RtpMidiHostDefinition const& definition);
    bool RemoveHostDefinition(_In_ GUID const& entryId);
    bool TryGetHostDefinition(_In_ GUID const& entryId, _Out_ RtpMidiHostDefinition& definition);
    bool SetHostEnabled(_In_ GUID const& entryId, _In_ bool const enabled);
    std::vector<RtpMidiHostDefinition> GetHostDefinitions();

    void SetClientDefinition(_In_ RtpMidiClientDefinition const& definition);
    bool RemoveClientDefinition(_In_ GUID const& entryId);
    bool TryGetClientDefinition(_In_ GUID const& entryId, _Out_ RtpMidiClientDefinition& definition);
    std::vector<RtpMidiClientDefinition> GetClientDefinitions();

    // Who may connect to each host. A host's state lives exactly as long as its definition.
    RtpMidiApprovals& Approvals() noexcept { return m_approvals; }

    // Each host's remotes with their own settings. Kept apart from the definition, so creating a
    // host again with the same id keeps them, and removed with the definition.
    void SetRemoteClientSettings(_In_ GUID const& hostId, _In_ std::vector<RtpMidiRemoteClientSettings> const& settings);
    std::vector<RtpMidiRemoteClientSettings> GetRemoteClientSettings(_In_ GUID const& hostId);

    // The settings this remote has of its own on the host, if it has them
    std::optional<RtpMidiRemoteClientSettings> FindRemoteClientSettings(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);

private:
    TransportState() = default;
    ~TransportState() = default;

    wil::srwlock m_managersLock;
    wil::com_ptr<CMidi2RtpMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2RtpMidiConfigurationManager> m_configurationManager;

    // taken before the approvals' own lock, never after it
    wil::srwlock m_definitionsLock;
    std::map<GUID, RtpMidiHostDefinition, GuidLess> m_hosts;
    std::map<GUID, RtpMidiClientDefinition, GuidLess> m_clients;
    std::map<GUID, std::vector<RtpMidiRemoteClientSettings>, GuidLess> m_remoteClientSettings;

    RtpMidiApprovals m_approvals;
};
