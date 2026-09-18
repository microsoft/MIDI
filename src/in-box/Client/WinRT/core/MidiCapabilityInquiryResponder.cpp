// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiCapabilityInquiryResponder.h"
#include "CapabilityInquiry.MidiCapabilityInquiryResponder.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    winrt::hstring MidiCapabilityInquiryResponder::ToString()
    {
        try
        {
            std::string categories{};

            if (SupportsProfiles()) { categories += " profiles"; }
            if (SupportsPropertyExchange()) { categories += " property exchange"; }
            if (SupportsProcessInquiry()) { categories += " process inquiry"; }

            if (categories.empty())
            {
                categories = " no categories";
            }

            return winrt::to_hstring(
                std::format("{}:{}",
                    m_muid == nullptr ? std::string{ "no identifier" } : winrt::to_string(m_muid.ToString()),
                    categories));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return L"";
        }
    }
}
