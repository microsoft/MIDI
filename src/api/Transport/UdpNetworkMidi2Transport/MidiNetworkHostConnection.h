// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// The half of a connection that only exists when Windows is the host: everything reachable by
// an unsolicited datagram from a remote we did not choose. Admission, identity, approval and
// endpoint creation live here, so the answer to "what can a stranger reach?" is one file.
class MidiNetworkHostConnection : public MidiNetworkConnection
{
public:
    HRESULT Initialize(
        _In_ winrt::guid const& configIdentifier,
        _In_ std::wstring const& hostParentInstanceId,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteClientHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly,
        _In_ MidiNetworkAuthenticationKind const authenticationKind,
        _In_ MidiNetworkCredentialIdentifier const& credentialIdentifier
    );

    // Called by the endpoint creation worker once the MIDI endpoint exists, to finish accepting
    // an invitation that was answered with Invitation Reply: Pending.
    HRESULT CompleteHostSessionAfterEndpointCreated(
        _In_ std::wstring const& newDeviceInstanceId,
        _In_ std::wstring const& newEndpointDeviceInterfaceId);

    // Performs the creation itself, on the worker rather than the receive callback.
    HRESULT CreateHostEndpointForPendingInvitation(
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId,
        _Out_ std::wstring& newDeviceInstanceId,
        _Out_ std::wstring& newEndpointDeviceInterfaceId);

    // Called by the same worker when the endpoint could not be created.
    HRESULT FailHostSessionEndpointCreation(_In_ HRESULT const failure);

    // The remote's own identity, as supplied in its invitation.
    MidiNetworkRemoteClientIdentity GetRemoteClientIdentity()
    {
        auto lock = m_remoteIdentityLock.lock();

        return MidiNetworkRemoteClientIdentity{ m_remoteEndpointName, m_remoteProductInstanceId };
    }

    // The remote has been answered with an Invitation Reply Pending and is waiting on a user
    // decision.
    bool IsAwaitingUserApproval() { return m_awaitingUserApproval; }

    // FILETIME, UTC, of the first invitation which put this remote into the pending state. Zero
    // when it has never been pending. The client re-invites on a timer while it waits, so this
    // deliberately records the first ask and not the most recent one.
    uint64_t GetUserApprovalRequestedFileTime() { return m_userApprovalRequestedFileTime.load(); }

    // The remote said Bye before its endpoint had been created, so the queued creation is no
    // longer wanted. Building it anyway costs a device node created and then immediately torn
    // down, and that teardown contends with the creations still queued.
    bool IsHostEndpointCreationAbandoned() { return m_hostEndpointCreationAbandoned; }

    // Drops a queued creation. The remote has already gone, so nothing is sent to it.
    void CancelPendingHostEndpointCreation()
    {
        m_hostEndpointCreationPending = false;
    }

    // A user allowed this pending remote. Resumes the invitation where the approval gate left it.
    HRESULT ApproveByUser();

    // A user refused this pending remote. Sends the Bye the spec has for exactly this.
    HRESULT DenyByUser();

protected:
    HRESULT HandleIncomingInvitation(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkCommandInvitationCapabilities const& capabilities,
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId) override;

    HRESULT HandleIncomingInvitationWithAuthentication(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkAuthenticationKind const kind) override;

    // Spec 6.4: a host declining. A client withdrawing its own invitation uses 0x80 instead.
    MidiNetworkCommandByeReason ByeReasonForDeviceAlreadyAttached() const noexcept override
    {
        return MidiNetworkCommandByeReason::CommandByeReasonHostToClient_TooManyOpenSessions;
    }

    // A Bye before the endpoint exists means the queued creation is pointless.
    void OnSessionEndedBeforeEndpointCreated() noexcept override
    {
        m_hostEndpointCreationAbandoned = true;
    }

private:
    // Identity the remote supplied in its invitation. This is what the user approves and what
    // the allow and deny lists match on. Written on the socket receive thread and read by the
    // configuration manager, so it is guarded.
    wil::critical_section m_remoteIdentityLock;
    std::wstring m_remoteEndpointName{ };
    std::wstring m_remoteProductInstanceId{ };

    // The remote has been told its invitation is pending and is waiting for a user to approve or
    // deny it. No endpoint exists yet.
    std::atomic<bool> m_awaitingUserApproval{ false };

    // When that wait started, so a user deciding later can see how long something has been
    // asking. Set on the transition into the pending state only.
    std::atomic<uint64_t> m_userApprovalRequestedFileTime{ 0 };

    // Set while an endpoint is being created for an invitation we answered with Pending, so a
    // repeated invitation does not queue the work a second time.
    std::atomic<bool> m_hostEndpointCreationPending{ false };

    // Set when a Bye arrives for a connection whose endpoint has not been created yet.
    std::atomic<bool> m_hostEndpointCreationAbandoned{ false };

    MidiNetworkAuthenticationKind m_authenticationKind{ MidiNetworkAuthenticationKind::None };
    MidiNetworkCredentialIdentifier m_credentialIdentifier{ };
};
