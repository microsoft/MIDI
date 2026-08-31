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
    void TransportItem::Update(
        winrt::hstring const& transportId,
        winrt::hstring const& name,
        winrt::hstring const& codeText,
        winrt::hstring const& description,
        winrt::hstring const& detailText,
        winrt::hstring const& moduleText,
        winrt::hstring const& statusText,
        Severity severity) noexcept
    {
        UpdateField(m_transportId, transportId, L"TransportId");
        UpdateField(m_name, name, L"Name");
        UpdateField(m_codeText, codeText, L"CodeText");
        UpdateField(m_statusText, statusText, L"StatusText");

        if (UpdateField(m_description, description, L"Description"))
        {
            RaisePropertyChanged(L"DescriptionVisibility");
        }

        UpdateField(m_detailText, detailText, L"DetailText");

        if (UpdateField(m_moduleText, moduleText, L"ModuleText"))
        {
            RaisePropertyChanged(L"ModuleVisibility");
        }

        auto const ok = severity == Severity::Ok ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        auto const warning = severity == Severity::Warning ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        auto const error = severity == Severity::Error ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;

        UpdateField(m_okVisibility, ok, L"OkVisibility");
        UpdateField(m_warningVisibility, warning, L"WarningVisibility");
        UpdateField(m_errorVisibility, error, L"ErrorVisibility");
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
