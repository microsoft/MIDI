// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiCiProgramList.h"
#include "Telemetry.h"

#include <algorithm>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;

namespace midikeyboard
{
    namespace
    {
        // A workstation can offer thousands of patches. This is what the list control will hold
        // without becoming unusable, not a limit the specification imposes.
        constexpr size_t MaximumProgramEntries = 4096;

        constexpr wchar_t ProgramListResourceName[] = L"ProgramList";

        std::wstring JoinTags(_In_ MidiProgramListEntry const& entry) noexcept
        {
            std::wstring joined{};

            try
            {
                for (auto const& tag : entry.Tags())
                {
                    if (tag.empty())
                    {
                        continue;
                    }

                    if (!joined.empty())
                    {
                        joined += L", ";
                    }

                    joined += tag;
                }
            }
            catch (...)
            {
            }

            return joined;
        }
    }


    MidiCiProgramListQuery::~MidiCiProgramListQuery() noexcept
    {
        Cancel();
    }

    _Use_decl_annotations_
    std::shared_ptr<MidiCiProgramListQuery> MidiCiProgramListQuery::Start(
        MidiEndpointConnection const& connection,
        uint8_t group,
        uint8_t channel,
        CompletedHandler handler) noexcept
    {
        try
        {
            if (connection == nullptr || handler == nullptr)
            {
                return nullptr;
            }

            auto query = std::make_shared<MidiCiProgramListQuery>();

            query->Begin(connection, group, channel, handler);

            return query;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::Begin(
        MidiEndpointConnection const& connection,
        uint8_t group,
        uint8_t channel,
        CompletedHandler handler) noexcept
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard(m_lock);

                m_group = group;
                m_channel = channel;
                m_handler = handler;

                m_session = MidiCapabilityInquirySession::Create(connection);

                if (m_session == nullptr)
                {
                    m_handler = nullptr;
                }
                else
                {
                    m_session.Group(MidiGroup(group));
                    m_session.ResponseTimeoutMilliseconds(StepTimeoutMilliseconds);
                }
            }

            if (m_session == nullptr)
            {
                handler(ProgramListResult::NoResponse, {});
                return;
            }

            // Held for the length of the exchange, so a caller that drops its reference the moment
            // Start returns does not pull the object out from under the worker.
            m_self = shared_from_this();

            auto const self = m_self;

            winrt::Windows::System::Threading::ThreadPool::RunAsync(
                [self](auto&&)
                {
                    self->Run();
                });
        }
        catch (...)
        {
            m_self.reset();

            if (handler != nullptr)
            {
                handler(ProgramListResult::NoResponse, {});
            }
        }
    }

    void MidiCiProgramListQuery::Run() noexcept
    {
        try
        {
            MidiCapabilityInquirySession session{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_lock);
                session = m_session;
            }

            if (session == nullptr || m_canceled)
            {
                Complete(ProgramListResult::NoResponse);
                return;
            }

            auto const responders = session.DiscoverAsync().get();

            if (m_canceled)
            {
                Complete(ProgramListResult::NoResponse);
                return;
            }

            if (responders == nullptr || responders.Size() == 0)
            {
                // Nothing answered Discovery, so this is not a capability inquiry device at all.
                Complete(ProgramListResult::NoResponse);
                return;
            }

            MidiCapabilityInquiryResponder responder{ nullptr };

            for (auto const& candidate : responders)
            {
                if (candidate != nullptr && candidate.SupportsPropertyExchange())
                {
                    responder = candidate;
                    break;
                }
            }

            if (responder == nullptr)
            {
                // It answered, but said it does not do property exchange, so there is nothing to
                // ask it for.
                Complete(ProgramListResult::NotSupported);
                return;
            }

            auto const muid = responder.Muid();

            auto const channelList = session.GetChannelListAsync(muid).get();

            if (m_canceled)
            {
                Complete(ProgramListResult::NoResponse);
                return;
            }

            std::vector<MidiResourceLink> links{};

            if (channelList != nullptr)
            {
                links = ProgramListLinksForChannel(channelList);
            }

            // More than one collection is worth labeling; a single one would just be noise.
            auto const labelWithCollection = links.size() > 1;

            int32_t added{ 0 };
            bool offeredAList{ false };

            if (links.empty())
            {
                // A device with one program list does not need a channel list to point at it, so
                // ask for the list directly rather than deciding it has none.
                auto const programList = session.GetProgramListAsync(muid, L"").get();

                if (programList != nullptr)
                {
                    offeredAList = true;
                    added += CollectPrograms(programList, L"", false);
                }
            }
            else
            {
                offeredAList = true;

                for (auto const& link : links)
                {
                    if (m_canceled || m_entries.size() >= MaximumProgramEntries)
                    {
                        break;
                    }

                    auto const programList = session.GetProgramListAsync(muid, link.ResourceId()).get();

                    if (programList == nullptr)
                    {
                        continue;
                    }

                    added += CollectPrograms(
                        programList, std::wstring{ link.Title() }, labelWithCollection);
                }
            }

            if (m_canceled)
            {
                Complete(ProgramListResult::NoResponse);
                return;
            }

            if (!offeredAList)
            {
                Complete(ProgramListResult::NotSupported);
                return;
            }

            Complete(added > 0 ? ProgramListResult::Success : ProgramListResult::Empty);
        }
        catch (...)
        {
            Complete(ProgramListResult::NoResponse);
        }
    }

    _Use_decl_annotations_
    std::vector<MidiResourceLink> MidiCiProgramListQuery::ProgramListLinksForChannel(
        MidiChannelList const& channelList) noexcept
    {
        std::vector<MidiResourceLink> links{};

        try
        {
            // The app holds a zero based channel index; the resource numbers channels from one.
            auto const entry = channelList.GetEntryForChannel(
                static_cast<uint16_t>(m_channel) + 1);

            if (entry == nullptr)
            {
                return links;
            }

            for (auto const& link : entry.Links())
            {
                if (link == nullptr || link.Resource() != ProgramListResourceName)
                {
                    continue;
                }

                if (link.ResourceId().empty())
                {
                    continue;
                }

                links.push_back(link);
            }
        }
        catch (...)
        {
        }

        return links;
    }

    _Use_decl_annotations_
    int32_t MidiCiProgramListQuery::CollectPrograms(
        MidiProgramList const& programList,
        std::wstring const& collectionTitle,
        bool const labelWithCollection) noexcept
    {
        int32_t added{ 0 };

        try
        {
            for (auto const& entry : programList.Entries())
            {
                if (m_entries.size() >= MaximumProgramEntries)
                {
                    break;
                }

                if (entry == nullptr || entry.Title().empty())
                {
                    continue;
                }

                ProgramListEntry program{};

                program.Title = entry.Title();
                program.Tags = JoinTags(entry);

                // These three go on the wire exactly as they arrive. M2-107-UM section 2.3 gives a
                // worked example: bankPC [121,2,49] is sent as Program Change 49.
                program.BankMsb = entry.BankMsb();
                program.BankLsb = entry.BankLsb();
                program.ProgramChange = entry.ProgramChange();

                if (labelWithCollection)
                {
                    program.CollectionTitle = collectionTitle;
                }

                m_entries.push_back(std::move(program));
                added++;
            }
        }
        catch (...)
        {
        }

        return added;
    }

    void MidiCiProgramListQuery::Cancel() noexcept
    {
        m_canceled = true;

        try
        {
            MidiCapabilityInquirySession session{ nullptr };

            {
                std::lock_guard<std::mutex> guard(m_lock);

                session = m_session;
                m_session = nullptr;
                m_handler = nullptr;
            }

            // Closing wakes anything waiting for a device that is never going to answer, so the
            // worker does not sit out the rest of its timeout before noticing.
            if (session != nullptr)
            {
                session.Close();
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::Complete(ProgramListResult result) noexcept
    {
        // The worker can reach this more than one way, and a cancel can arrive at any point, so
        // only the first caller through here gets to raise the handler.
        if (m_completed.exchange(true))
        {
            return;
        }

        CompletedHandler handler{};
        std::vector<ProgramListEntry> entries{};
        MidiCapabilityInquirySession session{ nullptr };

        try
        {
            {
                std::lock_guard<std::mutex> guard(m_lock);

                handler = m_handler;
                m_handler = nullptr;

                entries.swap(m_entries);

                session = m_session;
                m_session = nullptr;
            }

            if (session != nullptr)
            {
                session.Close();
            }

            if (handler != nullptr && !m_canceled)
            {
                handler(result, std::move(entries));
            }
        }
        catch (...)
        {
        }

        // Last thing: this may be the only reference left.
        m_self.reset();
    }
}
