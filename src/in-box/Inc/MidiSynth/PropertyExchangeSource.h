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

#include <sal.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MidiSynth
{
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
        const std::vector<char>& ProgramListJson() const noexcept { return m_programListJson; }

        // Reflects what is selected right now, which is why it is rebuilt per request and why it
        // is the one resource sent without a cache time.
        const std::vector<char>& RebuildChannelListJson(
            _In_ const SynthEngine& engine,
            _In_ const DlsCollection& collection);

        // The resource names advertised in the ResourceList, in the order they are serialized.
        static const char* const* ResourceNames(_Out_ size_t& count) noexcept;

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

        // Asking for something we do not have is answered, not ignored. An initiator that gets
        // silence waits out a timeout and may give up on the device entirely.
        void SendNotFound(
            _In_ IUmpOutput& output,
            _In_ uint8_t group,
            _In_ uint32_t sourceMuid,
            _In_ const UmpDispatcher::PendingPropertyRequest& request) noexcept;

    private:
        static std::string ToNarrow(_In_ const std::wstring& text);

        std::vector<char> m_resourceListJson;
        std::vector<char> m_deviceInfoJson;
        std::vector<char> m_programListJson;
        std::vector<char> m_channelListJson;

        WindowsMidiServicesCapabilityInquiry::PropertyReplyChunker m_chunker{};
        uint16_t m_nextChunk{ 0 };
        uint32_t m_replyInitiatorMuid{ 0 };
        uint8_t m_replyRequestId{ 0 };
    };
}
