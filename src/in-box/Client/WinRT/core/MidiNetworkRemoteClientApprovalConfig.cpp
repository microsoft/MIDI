// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkRemoteClientApprovalConfig.h"
#include "Transports.Network.MidiNetworkRemoteClientApprovalConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    _Use_decl_annotations_
    MidiNetworkRemoteClientApprovalConfig::MidiNetworkRemoteClientApprovalConfig(
        winrt::guid const& hostId, 
        winrt::hstring const& remoteClientName, 
        winrt::hstring const& remoteClientProductInstanceId, 
        bool const approve, 
        bool const restrictScopeToThisRequestOnly) noexcept
    {
        m_newClientId = foundation::GuidHelper::CreateNewGuid();

        m_hostId = hostId;
        m_remoteClientName = remoteClientName;
        m_remoteClientProductInstanceId = remoteClientProductInstanceId;
        m_approve = approve;
        m_scopeIsThisRequestOnly = restrictScopeToThisRequestOnly;
    }

    json::JsonObject MidiNetworkRemoteClientApprovalConfig::ConfigJson() noexcept
    {
        // An approval or denial is a command, not a configuration file update. The service is what
        // writes an approved client into the host's authorized client list, and only when the scope
        // is not restricted to the single pending request. There is therefore nothing for a client
        // to send here. ApproveOrDenyRemoteClientConnectRequestAsync is the supported path.
        //
        // An empty object is returned rather than null so that anything which round-trips this
        // through the generic IMidiServiceTransportPluginConfig surface does not fault.

        return json::JsonObject{};
    }

}
