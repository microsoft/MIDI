// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiResourceList.h"
#include "MidiResourceListEntry.h"
#include "CapabilityInquiry.MidiResourceList.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    _Use_decl_annotations_
    ci::MidiResourceListEntry MidiResourceList::GetEntry(winrt::hstring const& resource) noexcept
    {
        try
        {
            for (auto const& entry : m_entries)
            {
                // Resource names are case sensitive in the specification, so this does not fold
                // case: two names differing only in case are two different resources.
                if (entry != nullptr && entry.Resource() == resource)
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

    _Use_decl_annotations_
    bool MidiResourceList::SupportsResource(winrt::hstring const& resource) noexcept
    {
        return GetEntry(resource) != nullptr;
    }

    json::JsonArray MidiResourceList::GetJson() noexcept
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
    ci::MidiResourceList MidiResourceList::FromJson(json::JsonArray const& jsonArray) noexcept
    {
        auto list = winrt::make_self<implementation::MidiResourceList>();

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
                    implementation::MidiResourceListEntry::FromJson(element.GetObject()));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *list;
    }
}
