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
#include "OrphanedCustomizationItem.g.cpp"
#include "RelinkCandidateItem.g.cpp"
#include "TransportChoice.g.cpp"
#include "ConfigFileChoice.g.cpp"

#include "StringResources.h"

namespace res = ::midisettings::resources;

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

        RaisePropertyChanged(L"RowAccessibleName");
        RaisePropertyChanged(L"MonitorAccessibleName");
        RaisePropertyChanged(L"PanicAccessibleName");
    }

    winrt::hstring EndpointItem::RowAccessibleName() const noexcept
    {
        return m_description.empty()
            ? res::FormatString(
                L"EndpointRowAccessibleNameFormat",
                std::wstring{ m_name },
                std::wstring{ m_detailText })
            : res::FormatString(
                L"EndpointRowDescribedAccessibleNameFormat",
                std::wstring{ m_name },
                std::wstring{ m_description },
                std::wstring{ m_detailText });
    }

    winrt::hstring EndpointItem::MonitorAccessibleName() const noexcept
    {
        return res::FormatString(L"EndpointMonitorAccessibleNameFormat", std::wstring{ m_name });
    }

    winrt::hstring EndpointItem::PanicAccessibleName() const noexcept
    {
        return res::FormatString(L"EndpointPanicAccessibleNameFormat", std::wstring{ m_name });
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

        RaisePropertyChanged(L"RowAccessibleName");
    }

    winrt::hstring Midi1PortItem::RowAccessibleName() const noexcept
    {
        return res::FormatString(
            L"Midi1PortRowAccessibleNameFormat",
            std::wstring{ m_name },
            std::wstring{ m_detailText });
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

        m_originalCustomName = customName;
    }


    media::ImageSource OrphanedCustomizationItem::Image() const noexcept
    {
        // Same two decoders as the endpoint list: the shipped default art is SVG and the picture
        // a customer chose is usually not.
        if (m_image == nullptr && !m_imagePath.empty())
        {
            try
            {
                foundation::Uri const uri{ L"file:///" + m_imagePath };

                if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ m_imagePath }))
                {
                    media::Imaging::SvgImageSource source{};

                    source.RasterizePixelHeight(64);
                    source.UriSource(uri);

                    m_image = source;
                }
                else
                {
                    media::Imaging::BitmapImage bitmap{};

                    bitmap.DecodePixelHeight(64);
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
    void OrphanedCustomizationItem::Update(
        winrt::hstring const& displayName,
        winrt::hstring const& description,
        winrt::hstring const& contentSummary,
        winrt::hstring const& provenanceText,
        winrt::hstring const& storedId,
        winrt::hstring const& transportCode,
        winrt::hstring const& imagePath) noexcept
    {
        UpdateField(m_displayName, displayName, L"DisplayName");
        UpdateField(m_contentSummary, contentSummary, L"ContentSummary");
        UpdateField(m_storedId, storedId, L"StoredId");
        UpdateField(m_transportCode, transportCode, L"TransportCode");

        if (m_description != description)
        {
            m_description = description;
            RaisePropertyChanged(L"Description");
            RaisePropertyChanged(L"DescriptionVisibility");
        }

        if (m_provenanceText != provenanceText)
        {
            m_provenanceText = provenanceText;
            RaisePropertyChanged(L"ProvenanceText");
            RaisePropertyChanged(L"ProvenanceVisibility");
        }

        if (m_imagePath != imagePath)
        {
            m_imagePath = imagePath;
            m_image = nullptr;
            RaisePropertyChanged(L"Image");
        }
    }


    _Use_decl_annotations_
    void RelinkCandidateItem::Update(
        winrt::hstring const& name,
        winrt::hstring const& endpointDeviceId,
        winrt::hstring const& reasonText,
        bool const alreadyCustomized) noexcept
    {
        UpdateField(m_name, name, L"Name");
        UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId");

        if (m_reasonText != reasonText)
        {
            m_reasonText = reasonText;
            RaisePropertyChanged(L"ReasonText");
            RaisePropertyChanged(L"ReasonVisibility");
        }

        if (m_alreadyCustomized != alreadyCustomized)
        {
            m_alreadyCustomized = alreadyCustomized;
            RaisePropertyChanged(L"WarningVisibility");
        }
    }
}
