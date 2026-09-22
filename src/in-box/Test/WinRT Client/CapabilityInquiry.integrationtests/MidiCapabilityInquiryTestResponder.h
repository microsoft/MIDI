// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

// A device, for the session to talk to. It sits on the other end of a diagnostic loopback, decodes
// what arrives and answers it, which is the only way to test an initiator without hardware.
//
// It is deliberately simple-minded: it answers what a test tells it to answer, including answering
// wrongly or not at all, because half of what the session has to get right is what it does when a
// device misbehaves.
class MidiCapabilityInquiryTestResponder
{
public:

    MidiCapabilityInquiryTestResponder() = default;
    ~MidiCapabilityInquiryTestResponder() { Stop(); }

    void Start(
        winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
        uint32_t muid);

    void Stop();

    winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiUniqueId Muid() const { return m_muid; }

    // What to answer a Discovery with. Silence means the session should report nothing found.
    void AnswerDiscovery(bool value) { m_answerDiscovery = value; }

    // The property data to hand back, and the reply header status to hand back with it. A test
    // that wants a device which declines sets the status to something other than 200.
    void SetResource(std::string const& resource, std::string const& body);
    void SetResourceStatus(int32_t value) { m_resourceStatus = value; }

    // Answer every property request with a negative acknowledgment instead of a reply.
    void AnswerWithNak(bool value) { m_answerWithNak = value; }

    // Answer nothing at all, so the session has to time out.
    void AnswerNothing(bool value) { m_answerNothing = value; }

    // Pretend to be a device with a small system exclusive buffer, which is what forces chunking.
    void SetReceivableMaximumSystemExclusiveSize(uint32_t value) { m_maximumSystemExclusiveSize = value; }

    // A paged list. The responder slices this itself according to the offset and limit it is sent,
    // and reports the total, exactly as a workstation would.
    void SetPagedEntries(std::vector<std::string> const& entries);

    void SetProfiles(
        std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProfileId> const& enabled,
        std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProfileId> const& disabled);

    // Sends a report nobody asked for, to prove the session raises it as an event.
    void SendProfileEnabledReport(
        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProfileId const& profileId,
        uint16_t channelCount);

    uint32_t RequestCount() const { return m_requestCount; }

    // Every message type this responder was handed, in arrival order, so a test can assert what
    // the session sent and in what order.
    std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquiryMessageType>
        MessageLog() const;

private:

    void OnMessageReceived(
        winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args);

    void HandleMessage(
        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquiryMessage const& message);

    void HandlePropertyGet(
        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquiryMessage const& message);

    void Send(
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiMessage64> const& messages);

    winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
    winrt::event_token m_token{};

    winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiUniqueId m_muid{ nullptr };

    std::atomic<bool> m_answerDiscovery{ true };
    std::atomic<bool> m_answerWithNak{ false };
    std::atomic<bool> m_answerNothing{ false };
    std::atomic<int32_t> m_resourceStatus{ 200 };
    std::atomic<uint32_t> m_maximumSystemExclusiveSize{ 512 };
    std::atomic<uint32_t> m_requestCount{ 0 };

    mutable std::mutex m_lock;

    std::map<std::string, std::string> m_resources{};
    std::vector<std::string> m_pagedEntries{};

    std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProfileId> m_enabledProfiles{};
    std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProfileId> m_disabledProfiles{};

    std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquiryMessageType>
        m_messageLog{};

    std::vector<uint8_t> m_incoming{};
    bool m_incomingIsOpen{ false };
};
