// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

// Serves the MIDI-CI Property Exchange resources a General MIDI synthesizer offers.
//
// Free of WinRT on purpose. Windows.Data.Json is the only JSON parser allowed in Windows and it
// cannot be reached from this library, so the caller resolves the resource name out of the request
// header and hands back the blob it wants sent.

#pragma once

#include "DlsCollection.h"
#include "SynthEngine.h"
#include "UmpDispatcher.h"

#include "MidiCiMessage.h"
#include "MidiCiProgramList.h"

#include <sal.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MidiSynth
{
    // Resource ids for the two program lists. A channel's ChannelList entry links to exactly one
    // of them, which is what keeps a drum kit's bank and program from being offered on a melodic
    // channel and the other way round.
    constexpr char const* MelodicProgramListResourceId = "melodic";
    constexpr char const* DrumKitProgramListResourceId = "drums";

    class PropertyExchangeSource
    {
    public:
        // Serializes every static resource. Rebuilt only when the sound set changes, so answering
        // a request is a byte range slice rather than a build.
        //
        // The identity is the caller's so that this, the SysEx Identity Reply, the MIDI-CI
        // Discovery Reply and the UMP Device Identity Notification cannot disagree.
        void Build(_In_ const DlsCollection& collection, _In_ const SynthIdentity& identity);

        const std::vector<char>& ResourceListJson() const noexcept { return m_resourceListJson; }
        const std::vector<char>& DeviceInfoJson() const noexcept { return m_deviceInfoJson; }

        // A request naming neither list gets the melodic one. The resource list declares that a
        // resource id is required, but answering a client that did not send one is better than
        // refusing it.
        const std::vector<char>& ProgramListJson(_In_ const std::string& resourceId) const noexcept
        {
            return (resourceId == DrumKitProgramListResourceId)
                ? m_drumKitProgramListJson
                : m_melodicProgramListJson;
        }

        static bool IsKnownProgramListResourceId(_In_ const std::string& resourceId) noexcept
        {
            return resourceId.empty()
                || resourceId == MelodicProgramListResourceId
                || resourceId == DrumKitProgramListResourceId;
        }

        // Reflects what is selected right now, which is why it is rebuilt per request and why it
        // is the one resource sent without a cache time.
        const std::vector<char>& RebuildChannelListJson(
            _In_ const SynthEngine& engine,
            _In_ const DlsCollection& collection);

        // The resources advertised in the ResourceList, in the order they are serialized.
        static const WindowsMidiServicesCapabilityInquiry::ResourceListEntry* ResourceEntries(
            _Out_ size_t& count) noexcept;

        bool ReplyInProgress() const noexcept { return m_nextChunk != 0; }

        // The resource must outlive the reply. Every blob this class owns satisfies that.
        void BeginReply(
            _In_ const UmpDispatcher::PendingPropertyRequest& request,
            _In_ const std::vector<char>& resource,
            _In_ bool cacheable) noexcept;

        // Emits at most one chunk per call. A full program list is far more system exclusive
        // packets than an outbound queue holds at once, so the caller paces it.
        // Returns false when the reply has finished or none was started.
        bool SendNextChunk(
            _In_ IUmpOutput& output,
            _In_ uint8_t group,
            _In_ uint32_t sourceMuid) noexcept;

        void AbandonReply() noexcept { m_nextChunk = 0; }

        // ------------------------------------------------------------------- subscriptions
        //
        // Only ChannelList is subscribable: it is the one resource that changes while the device
        // is running. M2-103-UM section 11.
        //
        // A subscription has no meaning without something to send it on, so the caller parses the
        // command out of the header JSON and drives these.

        static constexpr size_t MaxSubscriptions = 8;
        static constexpr size_t MaxSubscribeIdBytes = 8;

        // Returns the identifier the responder assigned, or an empty string when there is no room
        // for another subscriber. Re-subscribing from the same initiator reuses its entry.
        const char* AddChannelListSubscription(_In_ uint32_t initiatorMuid) noexcept;

        // An empty identifier removes every subscription this initiator holds, which is what an
        // Invalidate MUID means.
        bool RemoveSubscription(_In_ uint32_t initiatorMuid, _In_ const std::string& subscribeId) noexcept;

        bool HasSubscriptions() const noexcept { return m_subscriptionCount > 0; }

        // True when the channel state has moved since the last update went out. Cheap enough to
        // call on every worker pass: it compares a sixty four byte snapshot, and only a real
        // change costs a rebuild.
        bool ChannelListChanged(_In_ const SynthEngine& engine) noexcept;

        // Starts sending the current ChannelList to one subscriber that has not had it yet.
        // Returns false when every subscriber is up to date. Drive it with SendNextChunk exactly
        // like a reply, and only while no other reply is in progress.
        bool BeginNextSubscriptionUpdate(
            _In_ const SynthEngine& engine,
            _In_ const DlsCollection& collection) noexcept;

        // Answers a subscription message. Status 200 carries the identifier back, which is what a
        // start needs; anything else is a refusal.
        void SendSubscriptionReply(
            _In_ IUmpOutput& output,
            _In_ uint8_t group,
            _In_ uint32_t sourceMuid,
            _In_ const UmpDispatcher::PendingPropertyRequest& request,
            _In_ uint16_t status,
            _In_z_ const char* subscribeId) noexcept;

        // Asking for something we do not have is answered, not ignored. An initiator that gets
        // silence waits out a timeout and may give up on the device entirely.
        void SendNotFound(
            _In_ IUmpOutput& output,
            _In_ uint8_t group,
            _In_ uint32_t sourceMuid,
            _In_ const UmpDispatcher::PendingPropertyRequest& request) noexcept;

    private:
        static std::string ToNarrow(_In_ const std::wstring& text);

        struct ChannelListSubscription
        {
            uint32_t InitiatorMuid{ 0 };
            char SubscribeId[MaxSubscribeIdBytes]{};
            bool NeedsUpdate{ false };
        };

        struct ChannelSnapshot
        {
            uint8_t BankMsb{ 0 };
            uint8_t BankLsb{ 0 };
            uint8_t Program{ 0 };
            bool IsDrumChannel{ false };
        };

        std::vector<char> m_resourceListJson;
        std::vector<char> m_deviceInfoJson;
        std::vector<char> m_melodicProgramListJson;
        std::vector<char> m_drumKitProgramListJson;
        std::vector<char> m_channelListJson;

        // The header for the update currently going out. It has to outlive the chunker, which
        // holds a pointer to it rather than a copy.
        char m_updateHeader[64]{};

        ChannelListSubscription m_subscriptions[MaxSubscriptions]{};
        size_t m_subscriptionCount{ 0 };
        uint32_t m_nextSubscribeId{ 1 };
        uint8_t m_nextUpdateRequestId{ 1 };

        ChannelSnapshot m_lastNotifiedChannels[MidiChannelCount]{};
        bool m_haveChannelSnapshot{ false };

        WindowsMidiServicesCapabilityInquiry::PropertyReplyChunker m_chunker{};
        uint16_t m_nextChunk{ 0 };
        uint32_t m_replyInitiatorMuid{ 0 };
        uint8_t m_replyRequestId{ 0 };
    };
}
