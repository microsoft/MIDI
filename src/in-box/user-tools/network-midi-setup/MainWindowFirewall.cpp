// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// The Windows Firewall page. The window never changes the firewall itself: it starts an elevated
// copy of this app to do that, so the app keeps running without administrator rights.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midinetworksetup::resources;
namespace firewall = ::midinetworksetup::firewall;

namespace winrt::midinetworksetup::implementation
{
    namespace
    {
        bool IsChecked(_In_ controls::CheckBox const& box) noexcept
        {
            try
            {
                auto const value = box.IsChecked();

                return value != nullptr && value.Value();
            }
            catch (...)
            {
                return false;
            }
        }

        long ChosenNetworks(_In_ controls::CheckBox const& privateBox, _In_ controls::CheckBox const& publicBox) noexcept
        {
            return
                (IsChecked(privateBox) ? firewall::PrivateNetworks : 0) |
                (IsChecked(publicBox) ? firewall::PublicNetworks : 0);
        }

        // Marks the type of network this PC is connected to now, so the customer knows which one
        // the firewall is blocking
        winrt::hstring NetworkChoiceText(_In_ std::wstring_view const resourceKey, _In_ bool const connectedNow) noexcept
        {
            auto const text = res::GetString(resourceKey);

            return connectedNow ? res::FormatString(L"FirewallConnectedNowFormat", text) : text;
        }

        winrt::hstring DescribeAllowResult(_In_ HRESULT const result) noexcept
        {
            try
            {
                if (SUCCEEDED(result))
                {
                    return res::GetString(L"FirewallAllowSucceeded");
                }

                if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
                {
                    return res::GetString(L"FirewallAllowCanceled");
                }

                if (result == HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST))
                {
                    return res::GetString(L"FirewallAllowServiceMissing");
                }

                // what the firewall's interface returns while its service is stopped, and Windows'
                // own text for it says nothing about a firewall
                if (result == HRESULT_FROM_WIN32(EPT_S_NOT_REGISTERED) ||
                    result == HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE))
                {
                    return res::GetString(L"FirewallAllowFirewallNotRunning");
                }

                auto reason = winrt::hresult_error{ result }.message();

                if (reason.empty())
                {
                    reason = winrt::hstring{ std::format(L"0x{:08X}", static_cast<uint32_t>(result)) };
                }

                return res::FormatString(L"FirewallAllowFailedFormat", reason);
            }
            catch (...)
            {
                return res::GetString(L"FirewallAllowFailed");
            }
        }
    }

    winrt::fire_and_forget MainWindow::RefreshFirewallStateAsync() noexcept
    {
        auto strongThis = get_strong();

        // the request reads the state again once it is done
        if (m_firewallBusy || m_firewallRefreshInProgress)
        {
            co_return;
        }

        m_firewallRefreshInProgress = true;

        try
        {
            winrt::apartment_context uiThread;

            co_await winrt::resume_background();

            auto const state = firewall::QueryState();

            co_await uiThread;

            m_firewallRefreshInProgress = false;

            if (!m_closing && !m_firewallBusy)
            {
                ApplyFirewallState(state);
            }
        }
        catch (...)
        {
            m_firewallRefreshInProgress = false;

            MIDI_NETSETUP_LOG_GENERAL_EXCEPTION(L"Unable to read the Windows Firewall state.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyFirewallState(firewall::FirewallState const& state) noexcept
    {
        try
        {
            m_firewallServiceFound = !state.ServicePath.empty();
            m_firewallOwnRuleNetworks = state.OwnRuleNetworks;

            winrt::hstring message{};
            auto severity = controls::InfoBarSeverity::Warning;

            switch (state.Status)
            {
            case firewall::FirewallStatus::Open:
                message = res::GetString(L"FirewallStateOpen");
                severity = controls::InfoBarSeverity::Success;
                break;

            case firewall::FirewallStatus::Closed:
                message = res::GetString(L"FirewallStateClosed");
                break;

            case firewall::FirewallStatus::ClosedByRule:
                message = res::GetString(L"FirewallStateClosedByRule");
                severity = controls::InfoBarSeverity::Error;
                break;

            case firewall::FirewallStatus::ClosedForAll:
                message = res::GetString(L"FirewallStateClosedForAll");
                severity = controls::InfoBarSeverity::Error;
                break;

            case firewall::FirewallStatus::Off:
                message = res::GetString(L"FirewallStateOff");
                severity = controls::InfoBarSeverity::Informational;
                break;

            case firewall::FirewallStatus::ServiceNotFound:
                message = res::GetString(L"FirewallStateServiceNotFound");
                severity = controls::InfoBarSeverity::Error;
                break;

            case firewall::FirewallStatus::NotConnected:
                message = res::GetString(L"FirewallStateNotConnected");
                severity = controls::InfoBarSeverity::Informational;
                break;

            default:
                message = res::GetString(L"FirewallStateUnknown");
                break;
            }

            if (state.ManagedByPolicy)
            {
                message = res::FormatString(L"FirewallStateManagedFormat", message);
            }

            FirewallStateBar().Severity(severity);
            FirewallStateBar().Message(message);
            FirewallStateBar().IsOpen(true);

            FirewallServicePathText().Text(state.ServicePath.empty() ?
                res::GetString(L"FirewallServicePathUnknown") :
                winrt::hstring{ state.ServicePath });

            FirewallPrivateCheckBox().Content(winrt::box_value(NetworkChoiceText(
                L"FirewallPrivateNetworks", (state.ConnectedNetworks & firewall::PrivateNetworks) != 0)));

            FirewallPublicCheckBox().Content(winrt::box_value(NetworkChoiceText(
                L"FirewallPublicNetworks", (state.ConnectedNetworks & firewall::PublicNetworks) != 0)));

            // Only the first reading sets the choices. After that they are the customer's.
            auto const readable =
                state.Status != firewall::FirewallStatus::Unknown &&
                state.Status != firewall::FirewallStatus::ServiceNotFound;

            if (!m_firewallChoicesSet && readable)
            {
                m_firewallChoicesSet = true;

                // Private only, unless this app's rule already says otherwise. That is also what
                // Windows offers first when an app asks to be let through.
                auto const networks = state.OwnRuleNetworks != 0 ? state.OwnRuleNetworks : firewall::PrivateNetworks;

                FirewallPrivateCheckBox().IsChecked((networks & firewall::PrivateNetworks) != 0);
                FirewallPublicCheckBox().IsChecked((networks & firewall::PublicNetworks) != 0);
            }

            UpdateFirewallControls();
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to show the Windows Firewall state.")
    }

    void MainWindow::UpdateFirewallControls() noexcept
    {
        try
        {
            auto const chosen = ChosenNetworks(FirewallPrivateCheckBox(), FirewallPublicCheckBox());

            // nothing to do while this app's rule already covers exactly what is chosen
            FirewallAllowButton().IsEnabled(
                !m_firewallBusy && m_firewallServiceFound && chosen != 0 && chosen != m_firewallOwnRuleNetworks);
            FirewallPrivateCheckBox().IsEnabled(!m_firewallBusy);
            FirewallPublicCheckBox().IsEnabled(!m_firewallBusy);
            FirewallProgressRing().IsActive(m_firewallBusy);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnFirewallNetworkChoiceChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        // XAML raises Checked while it is still applying the markup
        if (!m_loaded)
        {
            return;
        }

        UpdateFirewallControls();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnAllowThroughFirewallClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        if (m_firewallBusy)
        {
            co_return;
        }

        try
        {
            auto const networks = ChosenNetworks(FirewallPrivateCheckBox(), FirewallPublicCheckBox());

            if (networks == 0)
            {
                co_return;
            }

            // the administrator prompt is shown over this window
            auto const owner = m_chrome.WindowHandle();

            m_firewallBusy = true;
            UpdateFirewallControls();

            // not a fading status: the prompt can stay up for as long as the customer likes
            if (m_firewallStatusTimer != nullptr)
            {
                m_firewallStatusTimer.Stop();
            }

            FirewallStatusText().Opacity(1.0);
            FirewallStatusText().Text(res::GetString(L"FirewallAllowWaiting"));

            winrt::apartment_context uiThread;

            co_await winrt::resume_background();

            auto const result = firewall::RequestAllow(owner, networks);
            auto const state = firewall::QueryState();

            co_await uiThread;

            m_firewallBusy = false;

            if (m_closing)
            {
                co_return;
            }

            ApplyFirewallState(state);
            ShowTransientStatus(FirewallStatusText(), m_firewallStatusTimer, DescribeAllowResult(result));
        }
        catch (...)
        {
            m_firewallBusy = false;

            MIDI_NETSETUP_LOG_GENERAL_EXCEPTION(L"Unable to allow the MIDI service through Windows Firewall.");

            UpdateFirewallControls();
        }
    }
}
