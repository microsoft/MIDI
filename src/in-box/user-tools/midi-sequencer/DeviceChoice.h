// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "DeviceChoice.g.h"

namespace midisequencer
{
    // A device's picture at a size in device-independent pixels, made once and shared, because
    // track headers are built again often. SVG and bitmap files both work. UI thread only. Null
    // when there's no path.
    media::ImageSource DevicePicture(_In_ std::wstring const& path, _In_ int32_t size) noexcept;
}

namespace winrt::midisequencer::implementation
{
    struct DeviceChoice : DeviceChoiceT<DeviceChoice>
    {
        DeviceChoice() = default;

        DeviceChoice(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ media::ImageSource const& picture,
            _In_ winrt::hstring const& tip) :
            m_displayName(displayName),
            m_endpointDeviceId(endpointDeviceId),
            m_picture(picture),
            m_tip(tip.empty() ? nullptr : winrt::box_value(tip))
        {
        }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        media::ImageSource Picture() const noexcept { return m_picture; }

        xaml::Visibility PictureVisibility() const noexcept
        {
            return m_picture == nullptr ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        foundation::IInspectable Tip() const noexcept { return m_tip; }

    private:
        winrt::hstring m_displayName{};
        winrt::hstring m_endpointDeviceId{};
        media::ImageSource m_picture{ nullptr };
        foundation::IInspectable m_tip{ nullptr };
    };
}

namespace winrt::midisequencer::factory_implementation
{
    struct DeviceChoice : DeviceChoiceT<DeviceChoice, implementation::DeviceChoice>
    {
    };
}
