// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiChannelList.h"
#include "MidiChannelListEntry.h"
#include "CapabilityInquiry.MidiChannelList.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        constexpr std::wstring_view ProgramListResourceName{ L"ProgramList" };
    }

    _Use_decl_annotations_
    ci::MidiChannelListEntry MidiChannelList::GetEntryForChannel(uint16_t const oneBasedChannel) noexcept
    {
        try
        {
            for (auto const& entry : m_entries)
            {
                if (entry != nullptr && entry.Channel() == oneBasedChannel)
                {
                    return entry;
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return nullptr;
    }

    foundation::Collections::IVector<ci::MidiResourceLink> MidiChannelList::GetProgramListLinks() noexcept
    {
        auto results = winrt::single_threaded_vector<ci::MidiResourceLink>();

        try
        {
            winrt::hstring const programList{ ProgramListResourceName };

            for (auto const& entry : m_entries)
            {
                if (entry == nullptr) continue;

                for (auto const& link : entry.Links())
                {
                    if (link == nullptr || link.Resource() != programList)
                    {
                        continue;
                    }

                    // Channels commonly share one collection, so the same link arrives repeatedly.
                    // The resource identifier is what distinguishes them; an empty one means the
                    // device has a single list and every channel is pointing at it.
                    bool alreadyHave = false;

                    for (auto const& existing : results)
                    {
                        if (existing.ResourceId() == link.ResourceId())
                        {
                            alreadyHave = true;
                            break;
                        }
                    }

                    if (!alreadyHave)
                    {
                        results.Append(link);
                    }
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return results;
    }

    json::JsonArray MidiChannelList::GetJson() noexcept
    {
        try
        {
            json::JsonArray jsonArray{};

            for (auto const& entry : m_entries)
            {
                if (entry == nullptr) continue;

                auto const entryJson = entry.GetJson();

                if (entryJson != nullptr)
                {
                    jsonArray.Append(entryJson);
                }
            }

            return jsonArray;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    ci::MidiChannelList MidiChannelList::FromJson(json::JsonArray const& jsonArray) noexcept
    {
        auto list = winrt::make_self<implementation::MidiChannelList>();

        try
        {
            if (jsonArray == nullptr)
            {
                return *list;
            }

            for (auto const& element : jsonArray)
            {
                if (element.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                list->Entries().Append(
                    implementation::MidiChannelListEntry::FromJson(element.GetObject()));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *list;
    }
}
