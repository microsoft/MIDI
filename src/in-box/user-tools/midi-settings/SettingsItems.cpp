// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SettingsItems.h"

#include "EndpointItem.g.cpp"
#include "Midi1PortItem.g.cpp"
#include "Midi1PortNameItem.g.cpp"
#include "TransportChoice.g.cpp"
#include "ConfigFileChoice.g.cpp"

namespace winrt::midisettings::implementation
{
    media::ImageSource EndpointItem::Image() const noexcept
    {
        // Built on first use: the stored value is a file path, and a path string will not bind
        // to Image.Source. BitmapImage cannot render SVG, and the shipped default endpoint art
        // is SVG, so the two decoders are chosen by extension.
        if (m_image == nullptr && !m_imagePath.empty())
        {
            try
            {
                foundation::Uri const uri{ L"file:///" + m_imagePath };

                if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ m_imagePath }))
                {
                    media::Imaging::SvgImageSource source{};

                    source.RasterizePixelHeight(96);
                    source.UriSource(uri);

                    m_image = source;
                }
                else
                {
                    media::Imaging::BitmapImage bitmap{};

                    bitmap.DecodePixelHeight(96);
                    bitmap.UriSource(uri);

                    m_image = bitmap;
                }
            }
            catch (...)
            {
            }
        }

        return m_image;
    }

    _Use_decl_annotations_
    void EndpointItem::Update(
        winrt::hstring const& endpointDeviceId,
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& transportCode,
        winrt::hstring const& detailText,
        winrt::hstring const& imagePath,
        bool const canMonitor) noexcept
    {
        UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId");
        UpdateField(m_name, name, L"Name");
        UpdateField(m_transportCode, transportCode, L"TransportCode");
        UpdateField(m_detailText, detailText, L"DetailText");

        if (UpdateField(m_description, description, L"Description"))
        {
            RaisePropertyChanged(L"DescriptionVisibility");
        }

        if (m_imagePath != imagePath)
        {
            m_imagePath = imagePath;
            m_image = nullptr;

            RaisePropertyChanged(L"Image");
        }

        if (m_canMonitor != canMonitor)
        {
            m_canMonitor = canMonitor;

            RaisePropertyChanged(L"MonitorVisibility");
        }
    }

    _Use_decl_annotations_
    void Midi1PortItem::Update(
        winrt::hstring const& portDeviceId,
        winrt::hstring const& name,
        winrt::hstring const& detailText) noexcept
    {
        UpdateField(m_portDeviceId, portDeviceId, L"PortDeviceId");
        UpdateField(m_name, name, L"Name");
        UpdateField(m_detailText, detailText, L"DetailText");
    }

    _Use_decl_annotations_
    void Midi1PortNameItem::Update(
        uint8_t const groupIndex,
        winrt::hstring const& currentName,
        winrt::hstring const& legacyCompatibleName,
        winrt::hstring const& newStyleName,
        winrt::hstring const& customName) noexcept
    {
        if (m_groupIndex != groupIndex)
        {
            m_groupIndex = groupIndex;
            RaisePropertyChanged(L"GroupIndex");
        }

        // groups are 0-based in the API and 1-based everywhere a customer sees them
        UpdateField(m_groupNumberText, winrt::hstring{ std::to_wstring(groupIndex + 1) }, L"GroupNumberText");
        UpdateField(m_currentName, currentName, L"CurrentName");
        UpdateField(m_legacyCompatibleName, legacyCompatibleName, L"LegacyCompatibleName");
        UpdateField(m_newStyleName, newStyleName, L"NewStyleName");
        UpdateField(m_customName, customName, L"CustomName");
    }
}
