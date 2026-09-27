// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Transport-wide state shared by the endpoint and configuration managers.
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

private:
    TransportState() = default;
    ~TransportState() = default;

    std::mutex m_managersLock;
    wil::com_ptr<CMidi2RtpMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2RtpMidiConfigurationManager> m_configurationManager;

    std::mutex m_definitionsLock;
    std::map<GUID, RtpMidiHostDefinition, GuidLess> m_hosts;
    std::map<GUID, RtpMidiClientDefinition, GuidLess> m_clients;
};
