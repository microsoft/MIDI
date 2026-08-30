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
                co_await OfferRestartAsync(res::GetString(L"DriverChangeRestartMessage"));
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
                co_await OfferRestartAsync(res::GetString(L"DriverChangeRestartMessage"));
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

            if (result.RebootRequired)
            {
                co_await OfferRestartAsync(res::GetString(L"KorgRemoveRestartMessage"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to remove the KORG driver package.")
    }
}
