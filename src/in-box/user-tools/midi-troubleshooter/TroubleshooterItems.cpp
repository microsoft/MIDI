// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TroubleshooterItems.h"

#include "SessionConnectionItem.g.cpp"
#include "SessionItem.g.cpp"
#include "TransportItem.g.cpp"
#include "RegistryEntryItem.g.cpp"
#include "DriverDeviceItem.g.cpp"

namespace winrt::miditroubleshooter::implementation
{
    _Use_decl_annotations_
    void SessionConnectionItem::Update(
        winrt::hstring const& endpointDeviceId,
        winrt::hstring const& displayName,
        winrt::hstring const& countText,
        winrt::hstring const& connectedSinceText) noexcept
    {
        UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId");
        UpdateField(m_displayName, displayName, L"DisplayName");
        UpdateField(m_countText, countText, L"CountText");
        UpdateField(m_connectedSinceText, connectedSinceText, L"ConnectedSinceText");
    }

    _Use_decl_annotations_
    void SessionItem::Update(
        winrt::hstring const& sessionId,
        winrt::hstring const& title,
        winrt::hstring const& processText,
        winrt::hstring const& startedText,
        winrt::hstring const& connectionCountText) noexcept
    {
        UpdateField(m_sessionId, sessionId, L"SessionId");
        UpdateField(m_title, title, L"Title");
        UpdateField(m_processText, processText, L"ProcessText");
        UpdateField(m_startedText, startedText, L"StartedText");
        UpdateField(m_connectionCountText, connectionCountText, L"ConnectionCountText");
    }

    _Use_decl_annotations_
    void TransportItem::Update(Values const& values) noexcept
    {
        UpdateField(m_values.TransportId, values.TransportId, L"TransportId");
        UpdateField(m_values.Name, values.Name, L"Name");
        UpdateField(m_values.Code, values.Code, L"CodeText");
        UpdateField(m_values.Detail, values.Detail, L"DetailText");
        UpdateField(m_values.State, values.State, L"StateText");
        UpdateField(m_values.RowAccessibleName, values.RowAccessibleName, L"RowAccessibleName");
        UpdateField(m_values.ToggleText, values.ToggleText, L"ToggleText");
        UpdateField(m_values.ToggleAccessibleName, values.ToggleAccessibleName, L"ToggleAccessibleName");

        if (UpdateField(m_values.Description, values.Description, L"Description"))
        {
            RaisePropertyChanged(L"DescriptionVisibility");
        }

        if (UpdateField(m_values.Module, values.Module, L"ModuleText"))
        {
            RaisePropertyChanged(L"ModuleVisibility");
        }

        if (UpdateField(m_values.Reason, values.Reason, L"ReasonText"))
        {
            RaisePropertyChanged(L"ReasonVisibility");
        }

        if (m_values.StateSeverity != values.StateSeverity)
        {
            m_values.StateSeverity = values.StateSeverity;

            RaisePropertyChanged(L"SuccessVisibility");
            RaisePropertyChanged(L"CautionVisibility");
            RaisePropertyChanged(L"CriticalVisibility");
            RaisePropertyChanged(L"NeutralVisibility");
        }

        if (m_values.RegistryKeyName != values.RegistryKeyName)
        {
            m_values.RegistryKeyName = values.RegistryKeyName;

            RaisePropertyChanged(L"CanToggle");
            RaisePropertyChanged(L"ToggleVisibility");
        }

        m_values.Enabled = values.Enabled;
        m_values.Loaded = values.Loaded;
    }

    _Use_decl_annotations_
    void RegistryEntryItem::Initialize(
        winrt::hstring const& name,
        winrt::hstring const& value,
        winrt::hstring const& comment,
        uint32_t severity,
        winrt::hstring const& detail) noexcept
    {
        m_name = name;
        m_value = value;
        m_comment = comment;
        m_detail = detail;

        m_okVisibility = severity == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        m_warningVisibility = severity == 1 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        m_errorVisibility = severity >= 2 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
    }

    _Use_decl_annotations_
    void DriverDeviceItem::Initialize(
        winrt::hstring const& instanceId,
        winrt::hstring const& name,
        winrt::hstring const& detailText,
        winrt::hstring const& driverText,
        winrt::hstring const& problemText,
        bool canUseUniversalMidiPacketDriver,
        bool canUseClassicDriver) noexcept
    {
        m_instanceId = instanceId;
        m_name = name;
        m_detailText = detailText;
        m_driverText = driverText;
        m_problemText = problemText;
        m_canUseUmp = canUseUniversalMidiPacketDriver;
        m_canUseClassic = canUseClassicDriver;
    }
}
