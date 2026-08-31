// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// The half of a connection that only exists when Windows is the client: we chose the remote, so
// there is no admission or approval here. What there is instead is the invitation handshake and
// the decision about whether to try again after the remote goes away.
class MidiNetworkClientConnection : public MidiNetworkConnection
{
public:
    HRESULT Initialize(
        _In_ winrt::guid const& configIdentifier,
        _In_ winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
        _In_ winrt::Windows::Networking::HostName const& remoteHostHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::wstring const& thisEndpointName,
        _In_ std::wstring const& thisProductInstanceId,
        _In_ uint16_t const retransmitBufferMaxCommandPacketCount,
        _In_ uint8_t const maxForwardErrorCorrectionCommandPacketCount,
        _In_ bool createUmpEndpointsOnly
    );

    HRESULT SendInvitation();

    // An invitation in flight keeps the connection alive even though no session exists yet.
    bool IsSessionFinished() override
    {
        return MidiNetworkConnection::IsSessionFinished() && !m_invitation.IsPending();
    }

protected:
    HRESULT HandleIncomingInvitationReplyAccepted(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ std::wstring const& remoteHostUmpEndpointName,
        _In_ std::wstring const& remoteHostProductInstanceId) override;

    HRESULT HandleIncomingInvitationReplyPending() override;

    HRESULT HandleIncomingInvitationReplyAuthenticationRequired(
        _In_ MidiNetworkCommandPacketHeader const& header,
        _In_ MidiNetworkAuthenticationKind const kind) override;

    // Spec 6.4: a client withdrawing its own invitation.
    MidiNetworkCommandByeReason ByeReasonForDeviceAlreadyAttached() const noexcept override
    {
        return MidiNetworkCommandByeReason::CommandByeReasonClientToHost_InvitationCanceled;
    }

    // Spec 6.4 requires the invitation to be repeated until a reply arrives, and a Bye with
    // reason 0x80 once we give up.
    HRESULT OnWatchdogTick() override;

    // The remote host ended the session on its own, so this one is worth re-establishing.
    void OnSessionEndedByRemote() override;

    void OnInvitationAnswered() noexcept override { m_invitation.Answered(); }

private:
    HRESULT SendInvitationCommand();

    // Puts the definition back in front of the endpoint creator worker. Deliberate teardowns do
    // not call this.
    HRESULT RequestReconnect();

    MidiNetworkInvitationState m_invitation;
};
