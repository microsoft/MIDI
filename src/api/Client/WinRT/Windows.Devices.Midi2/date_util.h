// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef DATE_UTIL_H
#define DATE_UTIL_H

#include <ctime>
#include <sstream>

namespace WindowsMidiServicesInternal
{
    inline winrt::Windows::Foundation::DateTime DateTimeFromISO8601(_In_ winrt::hstring dateString)
    {
        foundation::DateTime convertedDate{ };

        // Parse the ISO 8601 format date string into a foundation::DateTime
        std::tm parsedDate{};

        std::istringstream dateStream(winrt::to_string(dateString));
        dateStream >> std::get_time(&parsedDate, "%Y-%m-%d");

        if (!dateStream.fail())
        {
            convertedDate = winrt::clock::from_time_t(std::mktime(&parsedDate));
        }

        return convertedDate;
    }
}

#endif
