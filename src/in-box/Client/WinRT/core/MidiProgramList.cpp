// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiProgramList.h"
#include "MidiProgramListEntry.h"
#include "CapabilityInquiry.MidiProgramList.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    int32_t MidiProgramList::TotalCount() const noexcept
    {
        if (m_totalCount >= 0)
        {
            return m_totalCount;
        }

        try
        {
            return m_offset + static_cast<int32_t>(m_entries.Size());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return 0;
        }
    }

    bool MidiProgramList::HasMoreEntries() const noexcept
    {
        try
        {
            return NextOffset() > 0;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    int32_t MidiProgramList::NextOffset() const noexcept
    {
        try
        {
            // An empty page means the device has nothing further to give, whatever it claimed the
            // total to be. Without this an over-reported total would page forever.
            if (m_entries.Size() == 0)
            {
                return 0;
            }

            auto const next = m_offset + static_cast<int32_t>(m_entries.Size());

            return (next < TotalCount()) ? next : 0;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return 0;
        }
    }

    json::JsonArray MidiProgramList::GetJson() noexcept
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
    ci::MidiProgramList MidiProgramList::FromJson(json::JsonArray const& jsonArray) noexcept
    {
        auto list = winrt::make_self<implementation::MidiProgramList>();

        try
        {
            if (jsonArray == nullptr)
            {
                return *list;
            }

            for (auto const& element : jsonArray)
            {
                // A non-object element is skipped rather than ending the list. One bad row should
                // not cost a customer every program after it.
                if (element.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                list->Entries().Append(
                    implementation::MidiProgramListEntry::FromJson(element.GetObject()));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *list;
    }
}
