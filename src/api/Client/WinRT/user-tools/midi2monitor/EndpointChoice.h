// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EndpointChoice.g.h"

namespace winrt::midi2monitor::implementation
{
    struct EndpointChoice : EndpointChoiceT<EndpointChoice>
    {
        EndpointChoice() = default;

        EndpointChoice(
            winrt::hstring const& displayName,
            winrt::hstring const& endpointDeviceId,
            winrt::hstring const& imagePath) :
            m_displayName(displayName),
            m_endpointDeviceId(endpointDeviceId),
            m_imagePath(imagePath)
        {
        }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring ImagePath() const noexcept { return m_imagePath; }

        xaml::Visibility ImageVisibility() const noexcept
        {
            return m_imagePath.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

    private:
        winrt::hstring m_displayName{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_imagePath{};
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct EndpointChoice : EndpointChoiceT<EndpointChoice, implementation::EndpointChoice>
    {
    };
}
