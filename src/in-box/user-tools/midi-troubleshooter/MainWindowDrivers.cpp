// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // The data context of a control inside a DataTemplate is the row it was realized for.
        miditroubleshooter::DriverDeviceItem ItemFromSender(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                if (element == nullptr)
                {
                    return nullptr;
                }

                return element.DataContext().try_as<miditroubleshooter::DriverDeviceItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        winrt::hstring DriverDescription(_In_ native::HardwareDeviceInfo const& device) noexcept
        {
            switch (device.CurrentDriver)
            {
            case native::DeviceDriverKind::UniversalMidiPacket:
                return res::GetString(L"DriverKindUmp");

            case native::DeviceDriverKind::ClassicUsbAudio:
                return res::GetString(L"DriverKindClassic");

            case native::DeviceDriverKind::UsbAudio2:
                return res::GetString(L"DriverKindUsbAudio2");

            case native::DeviceDriverKind::Vendor:
                return res::FormatString(L"DriverKindVendorFormat",
                    device.DriverProvider.empty() ?
                        res::GetString(L"ValueUnknown") : winrt::hstring{ device.DriverProvider });

            default:
                return res::GetString(L"DriverKindUnknown");
            }
        }

        void ApplyKorgPackageCard(
            _In_ std::vector<native::DriverPackageInfo> const& packages,
            _In_ controls::Button const& button,
            _In_ controls::TextBlock const& status,
            _In_ std::wstring_view const notInstalledKey,
            _In_ bool const elevated) noexcept
        {
            try
            {
                button.IsEnabled(!packages.empty() && elevated);

                if (packages.empty())
                {
                    status.Text(res::GetString(notInstalledKey));
                    return;
                }

                std::wstring names{};

                for (auto const& package : packages)
                {
                    if (!names.empty())
                    {
                        names += L", ";
                    }

                    names += package.PublishedName;

                    if (!package.DisplayName.empty())
                    {
                        names += L" (" + package.DisplayName + L")";
                    }
                }

                status.Text(res::FormatString(
                    L"KorgDriverFoundFormat",
                    static_cast<uint32_t>(packages.size()),
                    winrt::hstring{ names }));
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a driver package card.")
        }

        // Both Windows MIDI Services SDK applications and WinMM applications register a session:
        // wdmaud2.drv runs in the application's own process, so the service records the real
        // process. The session name is what tells the two apart. Blocking, so it runs on a
        // background thread.
        std::vector<std::wstring> ActiveMidiClientDescriptions() noexcept
        {
            std::vector<std::wstring> clients{};

            try
            {
                auto const sessions = midi2rept::MidiReporting::GetActiveSessions();

                if (sessions == nullptr)
                {
                    return clients;
                }

                for (auto const& session : sessions)
                {
                    auto const sessionName = session.SessionName();

                    std::wstring description{ sessionName.empty() ?
                        res::FormatString(
                            L"SessionProcessFormat",
                            session.ProcessName(),
                            static_cast<uint64_t>(session.ProcessId())) :
                        res::FormatString(
                            L"DriverChangeServiceRestartClientFormat",
                            session.ProcessName(),
                            static_cast<uint64_t>(session.ProcessId()),
                            sessionName) };

                    if (std::find(clients.begin(), clients.end(), description) == clients.end())
                    {
                        clients.push_back(std::move(description));
                    }
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to list the active MIDI sessions.")

            return clients;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::OfferDriverChangeFollowUpAsync(
        native::DriverChangeFollowUp const followUp)
    {
        auto lifetime = get_strong();

        try
        {
            switch (followUp)
            {
            case native::DriverChangeFollowUp::RestartWindows:
                ShowDriverFollowUp(
                    controls::InfoBarSeverity::Warning,
                    L"DriverFollowUpRestartWindowsTitle",
                    L"DriverChangeRestartMessage");

                co_await OfferRestartAsync(res::GetString(L"DriverChangeRestartMessage"));
                break;

            case native::DriverChangeFollowUp::RestartMidiService:
                ShowDriverFollowUp(
                    controls::InfoBarSeverity::Warning,
                    L"DriverFollowUpRestartServiceTitle",
                    L"DriverChangeServiceRestartMessage");

                co_await OfferServiceRestartAsync();
                break;

            case native::DriverChangeFollowUp::ReplugDevice:
                ShowDriverFollowUp(
                    controls::InfoBarSeverity::Warning,
                    L"DriverFollowUpReplugTitle",
                    L"DriverChangeReplugMessage");

                if (m_openDialog == nullptr)
                {
                    ReplugDeviceDialogText().Text(res::GetString(L"DriverChangeReplugMessage"));
                    ReplugDeviceDialog().XamlRoot(Content().XamlRoot());

                    m_openDialog = ReplugDeviceDialog();

                    co_await ReplugDeviceDialog().ShowAsync();

                    m_openDialog = nullptr;
                }
                break;

            default:
                ShowDriverFollowUp(
                    controls::InfoBarSeverity::Success,
                    L"DriverFollowUpNoneTitle",
                    L"DriverChangeNothingNeeded");
                break;
            }
        }
        catch (...)
        {
            m_openDialog = nullptr;

            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to offer the follow-up for a driver change.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowDriverFollowUp(
        controls::InfoBarSeverity const severity,
        std::wstring_view const titleKey,
        std::wstring_view const messageKey) noexcept
    {
        try
        {
            DriverFollowUpInfoBar().Severity(severity);
            DriverFollowUpInfoBar().Title(res::GetString(titleKey));
            DriverFollowUpInfoBar().Message(res::GetString(messageKey));
            DriverFollowUpInfoBar().IsOpen(true);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the driver change outcome.")
    }

    foundation::IAsyncAction MainWindow::OfferServiceRestartAsync()
    {
        auto lifetime = get_strong();

        if (m_openDialog != nullptr)
        {
            co_return;
        }

        try
        {
            std::vector<std::wstring> clients{};

            co_await native::RunOnBackgroundAsync([&clients]()
                {
                    clients = ActiveMidiClientDescriptions();
                });

            if (m_closing)
            {
                co_return;
            }

            std::wstring message{ res::GetString(L"DriverChangeServiceRestartMessage") };

            if (!clients.empty())
            {
                message += L"\r\n\r\n";
                message += res::GetString(L"DriverChangeServiceRestartClientsIntro");

                for (auto const& client : clients)
                {
                    message += L"\r\n\x2022 ";
                    message += client;
                }

                message += L"\r\n\r\n";
                message += res::GetString(L"DriverChangeServiceRestartClientsOutro");
            }

            RestartServiceDialogText().Text(winrt::hstring{ message });
            RestartServiceDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = RestartServiceDialog();

            auto const answer = co_await RestartServiceDialog().ShowAsync();

            m_openDialog = nullptr;

            // Later is a real answer. The driver is already in place either way.
            if (answer != controls::ContentDialogResult::Primary || m_closing)
            {
                co_return;
            }

            DriversProgressRing().IsActive(true);
            DriversStatusText().Text(res::GetString(L"ServiceRestarting"));

            native::ServiceOperationResult restart{};

            co_await native::RunOnBackgroundAsync([&restart]()
                {
                    restart = native::RestartMidiService();
                });

            if (m_closing)
            {
                co_return;
            }

            DriversProgressRing().IsActive(false);

            DriversStatusText().Text(restart.Succeeded ?
                res::GetString(L"ServiceRestarted") :
                res::FormatString(L"ServiceRestartFailedFormat", winrt::hstring{ restart.ErrorMessage }));
        }
        catch (...)
        {
            m_openDialog = nullptr;

            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to restart the service after a driver change.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyDriverDevices(
        std::vector<native::HardwareDeviceInfo> const& devices,
        std::vector<native::DriverPackageInfo> const& korgUsbPackages,
        std::vector<native::DriverPackageInfo> const& korgBlePackages) noexcept
    {
        try
        {
            m_driverDevices.Clear();

            for (auto const& device : devices)
            {
                auto item = winrt::make<DriverDeviceItem>();

                auto const driverText = res::FormatString(L"DriverCurrentFormat",
                    DriverDescription(device),
                    device.DriverInfName.empty() ?
                        res::GetString(L"ValueUnknown") : winrt::hstring{ device.DriverInfName },
                    device.DriverVersion.empty() ?
                        res::GetString(L"ValueUnknown") : winrt::hstring{ device.DriverVersion });

                winrt::get_self<DriverDeviceItem>(item)->Initialize(
                    winrt::hstring{ device.InstanceId },
                    device.Name.empty() ?
                        winrt::hstring{ device.InstanceId } : winrt::hstring{ device.Name },
                    winrt::hstring{ device.InstanceId },
                    driverText,
                    device.HasProblem ?
                        res::FormatString(L"DriverProblemFormat", static_cast<uint32_t>(device.ProblemCode)) :
                        winrt::hstring{},
                    device.CanUseUniversalMidiPacketDriver,
                    device.CanUseClassicDriver);

                m_driverDevices.Append(item);
            }

            m_korgUsbPackages = korgUsbPackages;
            m_korgBlePackages = korgBlePackages;

            ApplyKorgPackageCard(
                korgUsbPackages, RemoveKorgDriverButton(), KorgDriverStatusText(),
                L"KorgDriverNotInstalled", m_elevated);

            ApplyKorgPackageCard(
                korgBlePackages, RemoveKorgBleDriverButton(), KorgBleDriverStatusText(),
                L"KorgBleDriverNotInstalled", m_elevated);

            DriversStatusText().Text(devices.empty() ?
                res::GetString(L"DriversNoDevices") :
                res::FormatString(L"DriversDeviceCountFormat", static_cast<uint32_t>(devices.size())));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the driver devices.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRefreshDriversClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RefreshDriversButton().IsEnabled(false);
            DriversProgressRing().IsActive(true);
            DriversStatusText().Text(res::GetString(L"DriversScanning"));

            std::vector<native::HardwareDeviceInfo> devices{};
            std::vector<native::DriverPackageInfo> korgUsbPackages{};
            std::vector<native::DriverPackageInfo> korgBlePackages{};

            co_await native::RunOnBackgroundAsync([&devices, &korgUsbPackages, &korgBlePackages]()
                {
                    devices = native::EnumerateMidiHardwareDevices();
                    korgUsbPackages = native::FindKorgDriverPackages(native::KorgDriverKind::UsbMidi);
                    korgBlePackages = native::FindKorgDriverPackages(native::KorgDriverKind::BleMidi);
                });

            if (m_closing)
            {
                co_return;
            }

            RefreshDriversButton().IsEnabled(true);
            DriversProgressRing().IsActive(false);

            ApplyDriverDevices(devices, korgUsbPackages, korgBlePackages);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to enumerate the devices.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnUseUmpDriverClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        auto const item = ItemFromSender(sender);

        if (item == nullptr)
        {
            co_return;
        }

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"DriverChangeConfirmTitle"),
                res::FormatString(L"DriverChangeToUmpConfirmFormat", item.Name()));

            if (!confirmed)
            {
                co_return;
            }

            auto const instanceId = std::wstring{ item.InstanceId() };

            item.IsBusy(true);
            DriversProgressRing().IsActive(true);
            DriverFollowUpInfoBar().IsOpen(false);
            DriversStatusText().Text(res::GetString(L"DriverChanging"));

            native::DriverOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, &instanceId]()
                {
                    result = native::SetDeviceDriver(instanceId, native::DeviceDriverKind::UniversalMidiPacket);
                });

            if (m_closing)
            {
                co_return;
            }

            DriversProgressRing().IsActive(false);
            DriversStatusText().Text(winrt::hstring{ result.Message });

            OnRefreshDriversClick(nullptr, nullptr);

            if (result.Succeeded)
            {
                co_await OfferDriverChangeFollowUpAsync(result.FollowUp);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to switch the device to the new class driver.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnUseClassicDriverClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        auto const item = ItemFromSender(sender);

        if (item == nullptr)
        {
            co_return;
        }

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"DriverChangeConfirmTitle"),
                res::FormatString(L"DriverChangeToClassicConfirmFormat", item.Name()));

            if (!confirmed)
            {
                co_return;
            }

            auto const instanceId = std::wstring{ item.InstanceId() };

            item.IsBusy(true);
            DriversProgressRing().IsActive(true);
            DriverFollowUpInfoBar().IsOpen(false);
            DriversStatusText().Text(res::GetString(L"DriverChanging"));

            native::DriverOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, &instanceId]()
                {
                    result = native::SetDeviceDriver(instanceId, native::DeviceDriverKind::ClassicUsbAudio);
                });

            if (m_closing)
            {
                co_return;
            }

            DriversProgressRing().IsActive(false);
            DriversStatusText().Text(winrt::hstring{ result.Message });

            OnRefreshDriversClick(nullptr, nullptr);

            if (result.Succeeded)
            {
                co_await OfferDriverChangeFollowUpAsync(result.FollowUp);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to switch the device to the classic class driver.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRemoveKorgDriverClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        co_await RemoveKorgPackagesAsync(native::KorgDriverKind::UsbMidi);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRemoveKorgBleDriverClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        co_await RemoveKorgPackagesAsync(native::KorgDriverKind::BleMidi);
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::RemoveKorgPackagesAsync(native::KorgDriverKind const kind)
    {
        auto lifetime = get_strong();

        auto const isBle = kind == native::KorgDriverKind::BleMidi;

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const packages = isBle ? m_korgBlePackages : m_korgUsbPackages;

            if (packages.empty())
            {
                co_return;
            }

            std::wstring message{ res::GetString(L"KorgRemoveConfirmIntro") };

            for (auto const& package : packages)
            {
                message += L"\r\n\x2022 ";
                message += package.DisplayName.empty() ? package.PublishedName : package.DisplayName;
                message += L" - ";
                message += package.PublishedName;
            }

            message += L"\r\n\r\n";

            message += isBle ?
                res::GetString(L"KorgBleRemoveConfirmOutro") :
                res::GetString(L"KorgRemoveConfirmOutro");

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"KorgRemoveConfirmTitle"), winrt::hstring{ message });

            if (!confirmed)
            {
                co_return;
            }

            RemoveKorgDriverButton().IsEnabled(false);
            RemoveKorgBleDriverButton().IsEnabled(false);
            DriversProgressRing().IsActive(true);
            DriversStatusText().Text(res::GetString(L"KorgRemoving"));

            native::DriverOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, &packages]()
                {
                    result = native::RemoveDriverPackages(packages);
                });

            if (m_closing)
            {
                co_return;
            }

            DriversProgressRing().IsActive(false);

            std::wstring status{ result.Message };

            for (auto const& detail : result.Details)
            {
                status += L"  ";
                status += detail;
            }

            DriversStatusText().Text(winrt::hstring{ status });

            OnRefreshDriversClick(nullptr, nullptr);

            if (result.FollowUp == native::DriverChangeFollowUp::RestartWindows)
            {
                co_await OfferRestartAsync(res::GetString(L"KorgRemoveRestartMessage"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to remove the KORG driver package.")
    }
}
