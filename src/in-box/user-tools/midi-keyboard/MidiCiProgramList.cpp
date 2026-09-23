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

#include <MidiDefs.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;

namespace midikeyboard
{
    namespace
    {
        // A workstation can offer thousands of patches. This is what the list control will hold
        // without becoming unusable, not a limit the specification imposes.
        constexpr size_t MaximumProgramEntries = 4096;

        constexpr wchar_t ProgramListResourceName[] = L"ProgramList";
        constexpr wchar_t ChannelListResourceName[] = L"ChannelList";

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

        std::vector<std::wstring> CopyCategories(_In_ MidiProgramListEntry const& entry) noexcept
        {
            std::vector<std::wstring> categories{};

            try
            {
                for (auto const& category : entry.Categories())
                {
                    if (!category.empty())
                    {
                        categories.emplace_back(category);
                    }
                }
            }
            catch (...)
            {
            }

            return categories;
        }
    }

    _Use_decl_annotations_
    std::vector<ProgramCategoryGroup> GroupProgramsByCategory(
        std::vector<ProgramListEntry> const& entries,
        std::wstring const& otherName) noexcept
    {
        std::vector<ProgramCategoryGroup> groups{};

        try
        {
            std::vector<size_t> uncategorized{};

            for (size_t index = 0; index < entries.size(); index++)
            {
                auto const& entry = entries[index];

                if (entry.Categories.empty())
                {
                    uncategorized.push_back(index);
                    continue;
                }

                for (auto const& category : entry.Categories)
                {
                    auto existing = std::find_if(groups.begin(), groups.end(),
                        [&category](ProgramCategoryGroup const& group) { return group.Name == category; });

                    if (existing == groups.end())
                    {
                        groups.push_back(ProgramCategoryGroup{ category, { index } });
                    }
                    else
                    {
                        existing->EntryIndexes.push_back(index);
                    }
                }
            }

            // A device that categorizes nothing gets no grouped view at all, rather than one
            // group holding everything, which would be a worse way to show the same list.
            if (groups.empty())
            {
                return {};
            }

            if (!uncategorized.empty())
            {
                groups.push_back(ProgramCategoryGroup{ otherName, std::move(uncategorized) });
            }
        }
        catch (...)
        {
            return {};
        }

        return groups;
    }


    MidiCiProgramListQuery::~MidiCiProgramListQuery() noexcept
    {
        Cancel();
    }

    _Use_decl_annotations_
    std::shared_ptr<MidiCiProgramListQuery> MidiCiProgramListQuery::Start(
        MidiCapabilityInquirySession const& session,
        uint8_t channel,
        CompletedHandler handler,
        ChangedHandler changed) noexcept
    {
        try
        {
            if (session == nullptr || handler == nullptr)
            {
                return nullptr;
            }

            auto query = std::make_shared<MidiCiProgramListQuery>();

            query->Begin(session, channel, handler, changed);

            return query;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::Begin(
        MidiCapabilityInquirySession const& session,
        uint8_t channel,
        CompletedHandler handler,
        ChangedHandler changed) noexcept
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard(m_lock);

                m_channel = channel;
                m_handler = handler;
                m_changedHandler = changed;
                m_session = session;

                m_session.ResponseTimeoutMilliseconds(StepTimeoutMilliseconds);
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

            // Ask what the device offers before asking for any of it. A device with no ChannelList
            // would otherwise cost a full timeout to find that out, and this is also where the
            // device says whether a program list request has to name which list it wants.
            auto const resourceList = session.GetResourceListAsync(muid).get();

            if (m_canceled)
            {
                Complete(ProgramListResult::NoResponse);
                return;
            }

            // A device that publishes no ResourceList is not saying it has nothing. Only a list
            // that came back may be used to rule a resource out.
            bool offersChannelList{ true };
            bool channelListIsSubscribable{ false };

            if (resourceList != nullptr && resourceList.Entries().Size() > 0)
            {
                offersChannelList = resourceList.SupportsResource(ChannelListResourceName);

                if (!resourceList.SupportsResource(ProgramListResourceName))
                {
                    Complete(ProgramListResult::NotSupported);
                    return;
                }

                auto const channelEntry = resourceList.GetEntry(ChannelListResourceName);

                if (channelEntry != nullptr)
                {
                    channelListIsSubscribable = channelEntry.CanSubscribe();
                }
            }

            std::vector<MidiResourceLink> links{};

            if (offersChannelList)
            {
                auto const channelList = session.GetChannelListAsync(muid).get();

                if (m_canceled)
                {
                    Complete(ProgramListResult::NoResponse);
                    return;
                }

                if (channelList != nullptr)
                {
                    links = ProgramListLinksForChannel(channelList);
                }
            }

            {
                std::lock_guard<std::mutex> guard(m_lock);
                m_linkSignature = LinkSignature(links);
            }

            // More than one collection is worth labeling; a single one would just be noise.
            auto const labelWithCollection = links.size() > 1;

            int32_t added{ 0 };
            bool offeredAList{ false };

            if (links.empty())
            {
                // A device with one program list does not need a channel list to point at it, so
                // ask for the list directly rather than deciding it has none. Worth doing even
                // when the device declared that a resource id is required: with no link there is
                // no id to be had, and a refusal costs one exchange where giving up costs the
                // customer their program list.
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

            // Done before completing, because completing hands the session over to whatever is
            // keeping it open.
            if (channelListIsSubscribable)
            {
                WatchChannelList(session, muid);
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
    std::wstring MidiCiProgramListQuery::LinkSignature(
        std::vector<MidiResourceLink> const& links) noexcept
    {
        std::wstring signature{};

        try
        {
            for (auto const& link : links)
            {
                signature += std::wstring{ link.ResourceId() } + L"\n";
            }
        }
        catch (...)
        {
        }

        return signature;
    }

    _Use_decl_annotations_
    bool MidiCiProgramListQuery::ChannelLinksChanged(
        MidiPropertySubscriptionUpdatedEventArgs const& args) noexcept
    {
        try
        {
            if (args == nullptr || args.Update() == nullptr)
            {
                return true;
            }

            auto const body = args.Update().BodyAsJson();

            // A "notify" update says only that something changed, and a "partial" one carries a
            // fragment this cannot read as a channel list. Both mean re-ask.
            if (body == nullptr || body.ValueType() != winrt::Windows::Data::Json::JsonValueType::Array)
            {
                return true;
            }

            auto const channelList = MidiChannelList::FromJson(body.GetArray());

            if (channelList == nullptr)
            {
                return true;
            }

            auto const signature = LinkSignature(ProgramListLinksForChannel(channelList));

            std::lock_guard<std::mutex> guard(m_lock);

            if (signature == m_linkSignature)
            {
                return false;
            }

            m_linkSignature = signature;
            return true;
        }
        catch (...)
        {
            return true;
        }
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
                program.Categories = CopyCategories(entry);

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
            winrt::event_token token{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                session = m_session;
                token = m_subscriptionToken;

                m_session = nullptr;
                m_subscriptionToken = {};
                m_handler = nullptr;
                m_changedHandler = nullptr;
            }

            m_watching = false;

            // The session is borrowed and belongs to this app's presence on the connection, so it
            // is deliberately not closed here. Only the subscription this query took is given up.
            if (session != nullptr && token.value != 0)
            {
                session.PropertySubscriptionUpdated(token);
            }
        }
        catch (...)
        {
        }

        // A watching query keeps itself alive, so it has to let go here or nothing ever will.
        m_self.reset();
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::WatchChannelList(
        MidiCapabilityInquirySession const& session,
        MidiUniqueId const& muid) noexcept
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard(m_lock);

                if (m_changedHandler == nullptr)
                {
                    return;
                }
            }

            // Weak, so the subscription does not keep this object alive by itself. The query owns
            // its own lifetime through m_self and releases it in Cancel.
            std::weak_ptr<MidiCiProgramListQuery> weak = weak_from_this();

            auto const token = session.PropertySubscriptionUpdated(
                [weak](auto&&, MidiPropertySubscriptionUpdatedEventArgs const& args)
                {
                    auto const strong = weak.lock();

                    if (strong == nullptr || strong->m_canceled)
                    {
                        return;
                    }

                    ChangedHandler handler{};

                    {
                        std::lock_guard<std::mutex> guard(strong->m_lock);
                        handler = strong->m_changedHandler;
                    }

                    if (handler == nullptr)
                    {
                        return;
                    }

                    if (args != nullptr && args.IsSubscriptionEnded())
                    {
                        strong->m_watching = false;
                        handler();
                        return;
                    }

                    // A device sends an update for any change to the channel list, and a bank or
                    // program change on any channel is one. Almost all of those leave the
                    // collections this channel can select from alone, including the program change
                    // this app just sent, so re-fetching on every update would rebuild the list
                    // under the customer every time they picked a patch.
                    if (!strong->ChannelLinksChanged(args))
                    {
                        return;
                    }

                    handler();
                });

            auto const subscription = session.SubscribeAsync(muid, ChannelListResourceName, L"").get();

            if (subscription == nullptr || !subscription.IsActive())
            {
                session.PropertySubscriptionUpdated(token);
                return;
            }

            {
                std::lock_guard<std::mutex> guard(m_lock);
                m_subscriptionToken = token;
            }

            m_watching = true;
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

        try
        {
            {
                std::lock_guard<std::mutex> guard(m_lock);

                handler = m_handler;
                m_handler = nullptr;

                entries.swap(m_entries);
            }

            if (handler != nullptr && !m_canceled)
            {
                handler(result, std::move(entries));
            }
        }
        catch (...)
        {
        }

        // Last thing: this may be the only reference left. A query holding a subscription keeps it
        // until it is canceled, because the subscription needs this object alive to raise on.
        if (!m_watching)
        {
            m_self.reset();
        }
    }
}
