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

        // A PC which can't be on a domain network has no domain box, and its private box covers both,
        // so a rule made before the domain box existed still reads as chosen
        long PrivateBoxNetworks(_In_ bool const domainBoxShown) noexcept
        {
            return domainBoxShown ? firewall::PrivateNetworks : (firewall::DomainNetworks | firewall::PrivateNetworks);
        }

        long ChosenNetworks(
            _In_ controls::CheckBox const& domainBox,
            _In_ controls::CheckBox const& privateBox,
            _In_ controls::CheckBox const& publicBox,
            _In_ bool const domainBoxShown) noexcept
        {
            return
                (domainBoxShown && IsChecked(domainBox) ? firewall::DomainNetworks : 0) |
                (IsChecked(privateBox) ? PrivateBoxNetworks(domainBoxShown) : 0) |
                (IsChecked(publicBox) ? firewall::PublicNetworks : 0);
        }

        // Marks the type of network this PC is connected to now, so the customer knows which one
        // the firewall is blocking
        winrt::hstring NetworkChoiceText(_In_ std::wstring_view const resourceKey, _In_ bool const connectedNow) noexcept
        {
            auto const text = res::GetString(resourceKey);

            return connectedNow ? res::FormatString(L"FirewallConnectedNowFormat", text) : text;
        }

        // "a public network", for a network Windows gave no name
        winrt::hstring NetworkTypeText(_In_ long const type) noexcept
        {
            return res::GetString(
                type == firewall::DomainNetworks ? L"FirewallDomainNetworkType" :
                type == firewall::PrivateNetworks ? L"FirewallPrivateNetworkType" :
                L"FirewallPublicNetworkType");
        }

        // The check box for a type of network, as the status names it
        winrt::hstring NetworkChoiceName(_In_ long const type) noexcept
        {
            return res::GetString(
                type == firewall::DomainNetworks ? L"FirewallDomainNetworksShort" :
                type == firewall::PrivateNetworks ? L"FirewallPrivateNetworksShort" :
                L"FirewallPublicNetworksShort");
        }

        // "A", "A and B", or "A, B, and C"
        winrt::hstring JoinNames(_In_ std::vector<std::wstring> const& names)
        {
            if (names.size() < 2)
            {
                return names.empty() ? winrt::hstring{} : winrt::hstring{ names.front() };
            }

            if (names.size() == 2)
            {
                return res::FormatString(L"FirewallNamePairFormat", names[0], names[1]);
            }

            auto const separator = res::GetString(L"FirewallNameSeparator");

            std::wstring leading{ names.front() };

            for (size_t i = 1; i + 1 < names.size(); i++)
            {
                leading += separator;
                leading += names[i];
            }

            return res::FormatString(L"FirewallNameSeriesFormat", leading, names.back());
        }

        // Two networks can share a name, like two that Windows calls "Unidentified network"
        void AddName(_Inout_ std::vector<std::wstring>& names, _In_ std::wstring const& name)
        {
            auto const same = [&](std::wstring const& existing)
                {
                    return ::CompareStringOrdinal(existing.c_str(), -1, name.c_str(), -1, TRUE) == CSTR_EQUAL;
                };

            if (std::none_of(names.begin(), names.end(), same))
            {
                names.push_back(name);
            }
        }

        bool IsBlocked(_In_ firewall::FirewallStatus const status) noexcept
        {
            return
                status == firewall::FirewallStatus::Closed ||
                status == firewall::FirewallStatus::ClosedByRule ||
                status == firewall::FirewallStatus::ClosedForAll;
        }

        bool DescribesNetworks(_In_ firewall::FirewallStatus const status) noexcept
        {
            return IsBlocked(status) || status == firewall::FirewallStatus::Open || status == firewall::FirewallStatus::Off;
        }

        // The status, naming the networks it is about and saying what to do for a blocked one
        winrt::hstring DescribeFirewallState(_In_ firewall::FirewallState const& state, _Out_ controls::InfoBarSeverity& severity)
        {
            severity = controls::InfoBarSeverity::Warning;

            switch (state.Status)
            {
            case firewall::FirewallStatus::ServiceNotFound:
                severity = controls::InfoBarSeverity::Error;
                return res::GetString(L"FirewallStateServiceNotFound");

            case firewall::FirewallStatus::NotConnected:
                severity = controls::InfoBarSeverity::Informational;
                return res::GetString(L"FirewallStateNotConnected");

            case firewall::FirewallStatus::Unknown:
                return res::GetString(L"FirewallStateUnknown");

            default:
                break;
            }

            std::vector<std::wstring> passing{};
            std::vector<std::wstring> blocked{};
            std::vector<std::wstring> notAllowed{};
            long notAllowedTypes{ 0 };

            for (auto const& network : state.Networks)
            {
                std::wstring const name{ network.Name.empty() ? std::wstring{ NetworkTypeText(network.Type) } : network.Name };

                AddName(IsBlocked(network.Status) ? blocked : passing, name);

                if (network.Status == firewall::FirewallStatus::Closed)
                {
                    AddName(notAllowed, name);
                    notAllowedTypes |= network.Type;
                }
            }

            if (state.Status == firewall::FirewallStatus::Open)
            {
                severity = controls::InfoBarSeverity::Success;
                return res::FormatString(L"FirewallStateOpenFormat", JoinNames(passing));
            }

            if (state.Status == firewall::FirewallStatus::Off)
            {
                severity = controls::InfoBarSeverity::Informational;
                return res::FormatString(L"FirewallStateOffFormat", JoinNames(passing));
            }

            auto const summary = passing.empty() ?
                res::FormatString(L"FirewallStateClosedFormat", JoinNames(blocked)) :
                res::FormatString(L"FirewallStatePartlyClosedFormat", JoinNames(passing), JoinNames(blocked));

            if (state.Status == firewall::FirewallStatus::ClosedForAll)
            {
                severity = controls::InfoBarSeverity::Error;
                return res::FormatString(L"FirewallStateBlockAllFormat", summary);
            }

            if (state.Status == firewall::FirewallStatus::ClosedByRule)
            {
                severity = controls::InfoBarSeverity::Error;
                return res::FormatString(L"FirewallStateRuleFormat", summary);
            }

            std::vector<std::wstring> choices{};

            for (auto const type : { firewall::DomainNetworks, firewall::PrivateNetworks, firewall::PublicNetworks })
            {
                if ((notAllowedTypes & type) != 0)
                {
                    choices.emplace_back(NetworkChoiceName(type));
                }
            }

            return res::FormatString(L"FirewallStateAllowFormat", summary, JoinNames(notAllowed), JoinNames(choices));
        }

        // "Unidentified network: a public network, on vEthernet (WSL). Windows Firewall blocks the MIDI service."
        winrt::hstring DescribeNetwork(_In_ firewall::ConnectedNetwork const& network)
        {
            auto const name = network.Name.empty() ? res::GetString(L"FirewallNetworkWithoutName") : winrt::hstring{ network.Name };

            auto const status = res::GetString(
                network.Status == firewall::FirewallStatus::Open ? L"FirewallNetworkOpen" :
                network.Status == firewall::FirewallStatus::Off ? L"FirewallNetworkOff" :
                network.Status == firewall::FirewallStatus::ClosedByRule ? L"FirewallNetworkClosedByRule" :
                network.Status == firewall::FirewallStatus::ClosedForAll ? L"FirewallNetworkClosedForAll" :
                L"FirewallNetworkClosed");

            if (network.AdapterNames.empty())
            {
                return res::FormatString(L"FirewallNetworkNoAdapterFormat", name, NetworkTypeText(network.Type), status);
            }

            return res::FormatString(L"FirewallNetworkFormat", name, NetworkTypeText(network.Type), JoinNames(network.AdapterNames), status);
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
            m_firewallServiceFound = !state.ProgramPath.empty();
            m_firewallOwnRuleNetworks = state.OwnRuleNetworks;
            m_firewallShowDomain = state.JoinedToDomain || (state.ConnectedNetworks & firewall::DomainNetworks) != 0;

            auto severity = controls::InfoBarSeverity::Warning;
            auto message = DescribeFirewallState(state, severity);

            if (state.ManagedByPolicy)
            {
                message = res::FormatString(L"FirewallStateManagedFormat", message);
            }

            FirewallStateBar().Severity(severity);
            FirewallStateBar().Message(message);
            FirewallStateBar().IsOpen(true);

            // Only when Windows named the networks. Without names the status above already says it all.
            std::wstring lines{};

            if (DescribesNetworks(state.Status) &&
                std::any_of(state.Networks.begin(), state.Networks.end(), [](firewall::ConnectedNetwork const& network) { return !network.Name.empty(); }))
            {
                for (auto const& network : state.Networks)
                {
                    lines += lines.empty() ? L"" : L"\n";
                    lines += std::wstring_view{ DescribeNetwork(network) };
                }
            }

            FirewallNetworksText().Text(lines);
            FirewallNetworksPanel().Visibility(lines.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            FirewallServicePathText().Text(state.ProgramPath.empty() ?
                res::GetString(L"FirewallServicePathUnknown") :
                winrt::hstring{ state.ProgramPath });

            auto const privateBoxNetworks = PrivateBoxNetworks(m_firewallShowDomain);

            FirewallDomainCheckBox().Visibility(m_firewallShowDomain ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            FirewallDomainCheckBox().Content(winrt::box_value(NetworkChoiceText(
                L"FirewallDomainNetworks", (state.ConnectedNetworks & firewall::DomainNetworks) != 0)));

            FirewallPrivateCheckBox().Content(winrt::box_value(NetworkChoiceText(
                L"FirewallPrivateNetworks", (state.ConnectedNetworks & privateBoxNetworks) != 0)));

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

                FirewallDomainCheckBox().IsChecked(m_firewallShowDomain && (networks & firewall::DomainNetworks) != 0);
                FirewallPrivateCheckBox().IsChecked((networks & privateBoxNetworks) != 0);
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
            auto const chosen = ChosenNetworks(FirewallDomainCheckBox(), FirewallPrivateCheckBox(), FirewallPublicCheckBox(), m_firewallShowDomain);

            // nothing to do while this app's rule already covers exactly what is chosen
            FirewallAllowButton().IsEnabled(
                !m_firewallBusy && m_firewallServiceFound && chosen != 0 && chosen != m_firewallOwnRuleNetworks);
            FirewallDomainCheckBox().IsEnabled(!m_firewallBusy);
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
            auto const networks = ChosenNetworks(FirewallDomainCheckBox(), FirewallPrivateCheckBox(), FirewallPublicCheckBox(), m_firewallShowDomain);

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
