// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Serves the MIDI-CI Property Exchange resources the in-box General MIDI synthesizer serves, built
// from a SoundFont instead of from gm.dls. Adapted from MidiSynthLib's PropertyExchangeSource.
// Free of WinRT: the caller parses request headers with Windows.Data.Json.

#pragma once

#include "SoundFont.h"
#include "Synthesizer.h"
#include "SynthDispatcher.h"

#include "MidiCiMessage.h"
#include "MidiCiProgramList.h"

#include <string>
#include <vector>

namespace SoundFontSynth
{
    // A channel's ChannelList entry links to exactly one of these, so a kit is never offered on a
    // melodic channel or the other way round.
    constexpr char const* MelodicProgramListResourceId = "melodic";
    constexpr char const* DrumKitProgramListResourceId = "drums";

    enum class ProgramListKind
    {
        Melodic,
        DrumKits,
    };

    class SynthPropertySource
    {
    public:
        // Serializes every static resource once, so a request is answered with a byte range slice.
        void Build(_In_ SoundFont const& font, _In_ SynthIdentity const& identity, _In_z_ char const* modelName);

        std::vector<char> const& ResourceListJson() const noexcept { return m_resourceListJson; }
        std::vector<char> const& DeviceInfoJson() const noexcept { return m_deviceInfoJson; }

        // A request naming neither list gets the melodic one.
        std::vector<char> const& ProgramListJson(_In_ std::string const& resourceId) const noexcept
        {
            return (resourceId == DrumKitProgramListResourceId) ? m_drumKitProgramListJson : m_melodicProgramListJson;
        }

        static bool IsKnownProgramListResourceId(_In_ std::string const& resourceId) noexcept
        {
            return resourceId.empty() ||
                resourceId == MelodicProgramListResourceId ||
                resourceId == DrumKitProgramListResourceId;
        }

        size_t ProgramCount(_In_ ProgramListKind kind) const noexcept
        {
            return (kind == ProgramListKind::DrumKits) ? m_drumOrder.size() : m_melodicOrder.size();
        }

        // Reflects what is selected right now, so it is rebuilt per request and never cached.
        std::vector<char> const& RebuildChannelListJson(_In_ Synthesizer const& synthesizer, _In_ SoundFont const& font);

        static WindowsMidiServicesCapabilityInquiry::ResourceListEntry const* ResourceEntries(_Out_ size_t& count) noexcept;

        bool ReplyInProgress() const noexcept { return m_nextChunk != 0; }

        // The resource must outlive the reply. Every blob this class owns does.
        void BeginReply(
            _In_ SynthDispatcher::PendingPropertyRequest const& request,
            _In_ std::vector<char> const& resource,
            _In_ bool cacheable) noexcept;

        // The resource list declares ProgramList paginated, so every reply for it carries
        // "totalCount" (M2-103-UM section 8.6.2). SIZE_MAX means the request did not paginate.
        void BeginProgramListReply(
            _In_ SoundFont const& font,
            _In_ SynthDispatcher::PendingPropertyRequest const& request,
            _In_ std::string const& resourceId,
            _In_ size_t offset,
            _In_ size_t limit) noexcept;

        // One chunk per call, so the caller can pace a long reply. False when it has finished.
        bool SendNextChunk(_In_ ISysExSink& output, _In_ uint32_t sourceMuid) noexcept;

        void AbandonReply() noexcept { m_nextChunk = 0; }

        // Only ChannelList is subscribable: it is the one resource that changes while playing.
        static constexpr size_t MaximumSubscriptions = 8;
        static constexpr size_t MaximumSubscribeIdBytes = 8;

        // Empty when there is no room for another subscriber.
        char const* AddChannelListSubscription(_In_ uint32_t initiatorMuid) noexcept;

        // An empty identifier removes every subscription the initiator holds.
        bool RemoveSubscription(_In_ uint32_t initiatorMuid, _In_ std::string const& subscribeId) noexcept;

        bool HasSubscriptions() const noexcept { return m_subscriptionCount > 0; }

        bool ChannelListChanged(_In_ Synthesizer const& synthesizer) noexcept;

        // A "notify" rather than a "full": a sixteen channel list does not fit one chunk
        // (M2-103-UM section 11.1.1). The subscriber answers it with an ordinary Get.
        bool BeginNextSubscriptionNotification() noexcept;

        void SendSubscriptionReply(
            _In_ ISysExSink& output,
            _In_ uint32_t sourceMuid,
            _In_ SynthDispatcher::PendingPropertyRequest const& request,
            _In_ uint16_t status,
            _In_opt_z_ char const* subscribeId) noexcept;

        // Asking for something we do not have is answered, not ignored, or the initiator waits out
        // a timeout.
        void SendNotFound(
            _In_ ISysExSink& output,
            _In_ uint32_t sourceMuid,
            _In_ SynthDispatcher::PendingPropertyRequest const& request) noexcept;

    private:
        std::vector<char> BuildProgramListPage(
            _In_ SoundFont const& font,
            _In_ ProgramListKind kind,
            _In_ size_t offset,
            _In_ size_t limit) const;

        struct ChannelListSubscription
        {
            uint32_t InitiatorMuid{ 0 };
            char SubscribeId[MaximumSubscribeIdBytes]{};
            bool NeedsUpdate{ false };
        };

        std::vector<char> m_resourceListJson{};
        std::vector<char> m_deviceInfoJson{};
        std::vector<char> m_melodicProgramListJson{};
        std::vector<char> m_drumKitProgramListJson{};
        std::vector<char> m_channelListJson{};
        std::vector<char> m_programPageJson{};

        // Preset indices in the order the lists publish them.
        std::vector<uint32_t> m_melodicOrder{};
        std::vector<uint32_t> m_drumOrder{};

        // Only a bank that is laid out as General MIDI gets the General MIDI instrument groups.
        bool m_generalMidiLayout{ false };

        char m_replyHeader[80]{};
        char m_updateHeader[64]{};

        ChannelListSubscription m_subscriptions[MaximumSubscriptions]{};
        size_t m_subscriptionCount{ 0 };
        uint32_t m_nextSubscribeId{ 1 };
        uint8_t m_nextUpdateRequestId{ 1 };

        ChannelSelection m_lastNotifiedChannels[MidiChannelCount]{};
        bool m_haveChannelSnapshot{ false };

        WindowsMidiServicesCapabilityInquiry::PropertyReplyChunker m_chunker{};
        uint16_t m_nextChunk{ 0 };
        uint32_t m_replyInitiatorMuid{ 0 };
        uint8_t m_replyRequestId{ 0 };
    };

    // UTF-8, because the JSON writer escapes everything outside seven bit ASCII as the
    // specification asks.
    std::string ToUtf8(_In_ std::wstring const& text);
}
