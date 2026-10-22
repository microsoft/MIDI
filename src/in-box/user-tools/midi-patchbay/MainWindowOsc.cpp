// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// OSC connections in the patch window: adding one, its inspector, and the bar that says Windows
// Firewall keeps other devices out.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "DialogParts.h"
#include "OscAddress.h"
#include "OscDiscovery.h"
#include "OscHub.h"
#include "OscRules.h"
#include "OscText.h"
#include "PatchLibrary.h"
#include "SpinButtons.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

#include <midi_timestamp.h>

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;
namespace parts = ::midipatchbay::parts;
namespace firewall = ::midiapp::firewall;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // Wide enough for a host name and a port side by side.
        constexpr double OscDialogWidth = 520;

        // How often the firewall is read again while a patch wants every network.
        constexpr uint64_t FirewallRecheckSeconds = 30;

        media::Brush BrushOf(_In_ std::wstring_view key) noexcept
        {
            return patchbay::ThemeBrushes::Current().Get(key);
        }

        controls::TextBlock Line(_In_ winrt::hstring const& text, _In_ double size = 12, _In_ std::wstring_view brush = {})
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(xaml::TextWrapping::Wrap);

            if (!brush.empty())
            {
                block.Foreground(BrushOf(brush));
            }

            return block;
        }

        controls::StackPanel Section(_In_ winrt::hstring const& label)
        {
            controls::StackPanel panel{};

            panel.Spacing(6);
            panel.Children().Append(Line(label, 12, L"TextFillColorTertiaryBrush"));

            return panel;
        }

        controls::ComboBox Choices(_In_ winrt::hstring const& header, _In_ std::vector<winrt::hstring> const& items, _In_ int32_t selected)
        {
            controls::ComboBox box{};

            if (!header.empty())
            {
                box.Header(winrt::box_value(header));
            }

            for (auto const& item : items)
            {
                box.Items().Append(winrt::box_value(item));
            }

            box.SelectedIndex(selected);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            xaml::Automation::AutomationProperties::SetName(box, header);

            return box;
        }

        controls::NumberBox Port(_In_ winrt::hstring const& header, _In_ uint16_t value)
        {
            auto box = parts::NumberBox(header, 0, 65535, value, 100);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            return box;
        }

        uint16_t PortFrom(_In_ controls::NumberBox const& box) noexcept
        {
            auto const value = box.Value();
            return std::isfinite(value) ? static_cast<uint16_t>(std::clamp(value, 0.0, 65535.0)) : uint16_t{ 0 };
        }

        std::vector<winrt::hstring> TransportNames()
        {
            return {
                resources::GetString(L"OscTransportUdp"),
                resources::GetString(L"OscTransportTcpConnect"),
                resources::GetString(L"OscTransportTcpListen") };
        }

        std::vector<winrt::hstring> FramingNames()
        {
            return { resources::GetString(L"OscTcpFramingSlip"), resources::GetString(L"OscTcpFramingSizeFirst") };
        }

        winrt::hstring TransportHint(_In_ patchbay::OscTransport transport)
        {
            return resources::GetString(
                transport == patchbay::OscTransport::TcpConnect ? L"OscTransportTcpConnectHint" :
                transport == patchbay::OscTransport::TcpListen ? L"OscTransportTcpListenHint" :
                L"OscTransportUdpHint");
        }

        patchbay::OscTransport TransportFrom(_In_ controls::ComboBox const& box) noexcept
        {
            return static_cast<patchbay::OscTransport>(std::clamp(box.SelectedIndex(), 0, 2));
        }

        // What an app to send to advertises: one that waits for TCP connections, or one that
        // listens for UDP.
        std::wstring ServiceTypeOf(_In_ patchbay::OscTransport transport)
        {
            return transport == patchbay::OscTransport::TcpConnect ? patchbay::OscTcpServiceType : patchbay::OscUdpServiceType;
        }

        // A button listing the OSC apps found on the network. Picking one calls picked.
        controls::DropDownButton FindOnNetwork(
            _In_ std::function<std::wstring()> serviceType,
            _In_ std::function<void(patchbay::OscFoundApp const&)> picked)
        {
            controls::DropDownButton button{};
            button.Content(winrt::box_value(resources::GetString(L"OscFindButton")));
            xaml::Automation::AutomationProperties::SetName(button, resources::GetString(L"OscFindButton"));

            controls::MenuFlyout flyout{};
            flyout.Placement(controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

            // The list is read each time it opens, as apps come and go.
            flyout.Opening([serviceType, picked](winrt::Windows::Foundation::IInspectable const& sender, auto&&)
                {
                    try
                    {
                        auto const menu = sender.as<controls::MenuFlyout>();
                        menu.Items().Clear();

                        auto const apps = patchbay::OscDiscovery::Current().FoundApps(serviceType());

                        if (apps.empty())
                        {
                            controls::MenuFlyoutItem none{};
                            none.Text(resources::GetString(L"OscFindNone"));
                            none.IsEnabled(false);
                            menu.Items().Append(none);
                            return;
                        }

                        for (auto const& app : apps)
                        {
                            controls::MenuFlyoutItem item{};
                            item.Text(resources::FormatString(L"OscFindItemFormat", app.Name, app.Host, static_cast<uint32_t>(app.Port)));
                            item.Click([picked, app](auto&&, auto&&) { picked(app); });
                            menu.Items().Append(item);
                        }
                    }
                    MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the OSC apps on the network.")
                });

            button.Flyout(flyout);

            // Starts looking now, so the list has had a few seconds to fill when it's opened.
            (void)patchbay::OscDiscovery::Current().FoundApps(serviceType());

            return button;
        }

        std::vector<winrt::hstring> PresetNames()
        {
            std::vector<winrt::hstring> names{};

            for (int32_t i = 0; i < patchbay::OscPresetCount; i++)
            {
                names.push_back(patchbay::OscPresetName(static_cast<patchbay::OscPreset>(i)));
            }

            return names;
        }

        winrt::hstring PresetHint(_In_ patchbay::OscPreset preset)
        {
            switch (preset)
            {
            case patchbay::OscPreset::ReaperVirtualKeyboard: return resources::GetString(L"OscPresetReaperHint");
            case patchbay::OscPreset::Paths: return resources::GetString(L"OscPresetPathsHint");
            case patchbay::OscPreset::MidiType: return resources::GetString(L"OscPresetMidiTypeHint");
            case patchbay::OscPreset::OpenStageControl: return resources::GetString(L"OscPresetOpenStageControlHint");
            case patchbay::OscPreset::OscMidiOsc: return resources::GetString(L"OscPresetOscMidiOscHint");
            case patchbay::OscPreset::TouchOsc: return resources::GetString(L"OscPresetTouchOscHint");
            }

            return {};
        }

        // "OSC connection", or "OSC connection 2" when that's taken.
        std::wstring NewOscName(_In_ patchbay::PatchDocument const& patch)
        {
            std::wstring const base{ resources::GetString(L"OscDefaultName") };

            for (int32_t n = 1; n < 100; n++)
            {
                auto const name = n == 1 ? base : std::format(L"{} {}", base, n);

                if (std::none_of(patch.Endpoints.begin(), patch.Endpoints.end(),
                    [&name](patchbay::PatchEndpoint const& endpoint) { return endpoint.DisplayName == name; }))
                {
                    return name;
                }
            }

            return base;
        }
    }

    _Use_decl_annotations_
    bool MainWindow::ChangeOscConnection(
        std::wstring const& endpointId,
        std::function<void(patchbay::OscConnectionSettings&)> const& change,
        bool rebuildInspector) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();
            auto* target = patch == nullptr ? nullptr : patch->FindEndpoint(endpointId);

            if (target == nullptr || !target->IsOsc())
            {
                return false;
            }

            auto settings = *target->Osc;

            change(settings);

            // Through the file format and back, so what is kept is what a saved patch reads back as.
            settings = patchbay::OscConnectionFromJson(patchbay::OscConnectionToJson(settings));

            if (patchbay::OscConnectionSignature(settings) == patchbay::OscConnectionSignature(*target->Osc))
            {
                return false;
            }

            *target->Osc = std::move(settings);

            // The inspector stays as it is, so the control being used keeps its focus.
            CommitChange(true, false);
            RebuildCanvas();

            if (rebuildInspector)
            {
                RefreshInspector();
            }

            UpdateOscFirewallBar();

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the OSC connection.")

        return false;
    }

    winrt::fire_and_forget MainWindow::ShowAddOscConnectionDialogAsync(foundation::Point point)
    {
        auto strong = get_strong();

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            if (patch->OscConnectionCount() >= patchbay::MaximumOscConnectionsPerPatch)
            {
                ShowStatus(resources::GetString(L"OscConnectionLimit"), controls::InfoBarSeverity::Warning);
                co_return;
            }

            controls::ContentDialog dialog{};
            dialog.XamlRoot(Content().XamlRoot());

            // A dialog made in code gets the old look unless it's given the default style.
            if (auto const style = xaml::Application::Current().Resources().TryLookup(winrt::box_value(L"DefaultContentDialogStyle")))
            {
                dialog.Style(style.try_as<xaml::Style>());
            }

            dialog.RequestedTheme(RootGrid().ActualTheme());
            dialog.Title(winrt::box_value(resources::GetString(L"OscAddTitle")));
            dialog.PrimaryButtonText(resources::GetString(L"OscAddButton"));
            dialog.CloseButtonText(resources::GetString(L"OscCancelButton"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            controls::StackPanel body{};
            body.Spacing(12);
            body.Width(OscDialogWidth);

            body.Children().Append(parts::Hint(resources::GetString(L"OscAddIntro")));

            controls::TextBox nameBox{};
            nameBox.Header(winrt::box_value(resources::GetString(L"OscNameHeader")));
            nameBox.Text(winrt::hstring{ NewOscName(*patch) });
            nameBox.MaxLength(static_cast<int32_t>(patchbay::MaximumStringLength));
            body.Children().Append(nameBox);

            auto const initialPreset = patchbay::OscPreset::Paths;
            auto presetBox = Choices(resources::GetString(L"OscPresetHeader"), PresetNames(), static_cast<int32_t>(initialPreset));
            body.Children().Append(presetBox);

            auto presetHint = parts::HeadingHint(PresetHint(initialPreset));
            body.Children().Append(presetHint);

            // How it connects.
            controls::Grid transportRow{};
            transportRow.ColumnSpacing(12);

            for (int i = 0; i < 2; i++)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                transportRow.ColumnDefinitions().Append(column);
            }

            auto transportBox = Choices(resources::GetString(L"OscTransportHeader"), TransportNames(), 0);
            controls::Grid::SetColumn(transportBox, 0);
            transportRow.Children().Append(transportBox);

            auto framingBox = Choices(resources::GetString(L"OscTcpFramingHeader"), FramingNames(), 0);
            framingBox.IsEnabled(false);
            controls::Grid::SetColumn(framingBox, 1);
            transportRow.Children().Append(framingBox);

            body.Children().Append(transportRow);

            auto transportHint = parts::HeadingHint(TransportHint(patchbay::OscTransport::Udp));
            body.Children().Append(transportHint);

            // Receiving.
            body.Children().Append(parts::Heading(resources::GetString(L"OscReceiveHeading")));

            controls::Grid receiveRow{};
            receiveRow.ColumnSpacing(12);

            for (int i = 0; i < 2; i++)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                receiveRow.ColumnDefinitions().Append(column);
            }

            auto const initial = patchbay::OscConnectionFromPreset(initialPreset);

            auto receivePort = Port(resources::GetString(L"OscReceivePortHeader"), initial.ReceivePort);
            controls::Grid::SetColumn(receivePort, 0);
            receiveRow.Children().Append(receivePort);

            auto listenOn = Choices(resources::GetString(L"OscListenOnHeader"),
                { resources::GetString(L"OscListenEveryNetwork"), resources::GetString(L"OscListenThisPc") }, 0);
            controls::Grid::SetColumn(listenOn, 1);
            receiveRow.Children().Append(listenOn);

            body.Children().Append(receiveRow);
            body.Children().Append(parts::HeadingHint(resources::GetString(L"OscReceivePortHint")));

            // Sending.
            body.Children().Append(parts::Heading(resources::GetString(L"OscSendHeading")));

            controls::Grid sendRow{};
            sendRow.ColumnSpacing(12);

            {
                controls::ColumnDefinition wide{};
                wide.Width(xaml::GridLength{ 2, xaml::GridUnitType::Star });
                sendRow.ColumnDefinitions().Append(wide);

                controls::ColumnDefinition narrow{};
                narrow.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                sendRow.ColumnDefinitions().Append(narrow);
            }

            controls::TextBox hostBox{};
            hostBox.Header(winrt::box_value(resources::GetString(L"OscSendHostHeader")));
            hostBox.PlaceholderText(resources::GetString(L"OscSendHostPlaceholder"));
            hostBox.Text(winrt::hstring{ initial.SendHost });
            hostBox.MaxLength(static_cast<int32_t>(patchbay::MaximumOscHostLength));
            controls::Grid::SetColumn(hostBox, 0);
            sendRow.Children().Append(hostBox);

            auto sendPort = Port(resources::GetString(L"OscSendPortHeader"), initial.SendPort);
            controls::Grid::SetColumn(sendPort, 1);
            sendRow.Children().Append(sendPort);

            body.Children().Append(sendRow);
            body.Children().Append(parts::HeadingHint(resources::GetString(L"OscSendHostHint")));

            auto findButton = FindOnNetwork(
                [transportBox]() { return ServiceTypeOf(TransportFrom(transportBox)); },
                [hostBox, sendPort](patchbay::OscFoundApp const& app)
                {
                    hostBox.Text(winrt::hstring{ app.Host });
                    sendPort.Value(app.Port);
                });
            body.Children().Append(findButton);

            controls::StackPanel testRow{};
            testRow.Orientation(controls::Orientation::Horizontal);
            testRow.Spacing(10);

            controls::Button testButton{};
            testButton.Content(winrt::box_value(resources::GetString(L"OscTestButton")));
            testRow.Children().Append(testButton);

            auto testResult = Line({}, 12, L"TextFillColorSecondaryBrush");
            testResult.VerticalAlignment(xaml::VerticalAlignment::Center);
            testRow.Children().Append(testResult);

            body.Children().Append(testRow);

            midiapp::UseTouchSpinButtons({ receivePort, sendPort });

            // Only what the protocol uses can be changed.
            auto const applyTransport = [=]()
                {
                    auto const transport = TransportFrom(transportBox);

                    transportHint.Text(TransportHint(transport));
                    framingBox.IsEnabled(transport != patchbay::OscTransport::Udp);
                    receivePort.IsEnabled(transport != patchbay::OscTransport::TcpConnect);
                    listenOn.IsEnabled(transport != patchbay::OscTransport::TcpConnect);
                    hostBox.IsEnabled(transport != patchbay::OscTransport::TcpListen);
                    sendPort.IsEnabled(transport != patchbay::OscTransport::TcpListen);
                    findButton.IsEnabled(transport != patchbay::OscTransport::TcpListen);
                    testButton.IsEnabled(transport != patchbay::OscTransport::TcpListen);
                };

            transportBox.SelectionChanged([applyTransport](auto&&, auto&&) { applyTransport(); });

            auto const readSettings = [=]()
                {
                    auto const preset = static_cast<patchbay::OscPreset>(std::clamp(presetBox.SelectedIndex(), 0, patchbay::OscPresetCount - 1));
                    auto settings = patchbay::OscConnectionFromPreset(preset);

                    settings.Transport = TransportFrom(transportBox);
                    settings.TcpFraming = framingBox.SelectedIndex() == 1 ? patchbay::OscTcpFraming::SizeFirst : patchbay::OscTcpFraming::Slip;
                    settings.ReceivePort = PortFrom(receivePort);
                    settings.ListenOn = listenOn.SelectedIndex() == 1 ? patchbay::OscListenOn::ThisPc : patchbay::OscListenOn::EveryNetwork;
                    settings.SendHost = patchbay::SanitizeStoredString(std::wstring{ hostBox.Text() });
                    settings.SendPort = PortFrom(sendPort);

                    patchbay::SanitizeOscConnection(settings);

                    return settings;
                };

            presetBox.SelectionChanged([=](auto&&, auto&&)
                {
                    auto const preset = static_cast<patchbay::OscPreset>(std::clamp(presetBox.SelectedIndex(), 0, patchbay::OscPresetCount - 1));
                    auto const settings = patchbay::OscConnectionFromPreset(preset);

                    presetHint.Text(PresetHint(preset));
                    receivePort.Value(settings.ReceivePort);
                    sendPort.Value(settings.SendPort);

                    if (!settings.SendHost.empty())
                    {
                        hostBox.Text(winrt::hstring{ settings.SendHost });
                    }
                });

            testButton.Click([=](auto&&, auto&&) -> winrt::fire_and_forget
                {
                    auto const settings = readSettings();

                    if (!patchbay::OscConnectionSends(settings) || settings.SendHost.empty())
                    {
                        testResult.Text(resources::GetString(L"OscTestNeedsHost"));
                        co_return;
                    }

                    testButton.IsEnabled(false);
                    testResult.Text(resources::GetString(L"OscTestSending"));

                    bool sent{ false };

                    co_await patchbay::RunOnBackgroundAsync([&sent, settings]()
                        {
                            sent = patchbay::OscHub::SendTestNote(settings, settings.Rows.empty() ? 0 : settings.Rows.front().Group);
                        });

                    testButton.IsEnabled(true);
                    testResult.Text(resources::GetString(sent ? L"OscTestSent" : L"OscTestFailed"));
                });

            controls::ScrollViewer scroller{};
            scroller.Content(body);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            dialog.Content(scroller);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary || m_closing)
            {
                co_return;
            }

            auto* current = CurrentPatch();

            if (current == nullptr)
            {
                co_return;
            }

            patchbay::PatchEndpoint added{};
            added.DisplayName = patchbay::SanitizeStoredString(std::wstring{ nameBox.Text() });

            if (added.DisplayName.empty())
            {
                added.DisplayName = NewOscName(*current);
            }

            added.Osc = readSettings();

            AddRememberedEndpointToPatch(added, point);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add an OSC connection.")
    }
}

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // The settings a row's preset leaves alone.
        patchbay::OscRow WithPreset(_In_ patchbay::OscRow const& row, _In_ patchbay::OscPreset preset)
        {
            auto updated = patchbay::OscRowFromPreset(preset, row.Group);

            // A root the customer chose stays, unless the new format can't use it. Rules have none.
            if (updated.Format != patchbay::OscFormat::Custom &&
                (!row.Root.empty() || preset == patchbay::OscPreset::OpenStageControl || preset == patchbay::OscPreset::OscMidiOsc))
            {
                updated.Root = preset == patchbay::OscPreset::ReaperVirtualKeyboard ? updated.Root : row.Root;
            }

            if (updated.Format == patchbay::OscFormat::MidiType && updated.Root.empty())
            {
                updated.Root = "/midi";
            }

            return updated;
        }

        // A root for a new row that no row of the same format uses yet: /midi, /midi2...
        std::string FreeRoot(_In_ patchbay::OscConnectionSettings const& settings)
        {
            for (int32_t n = 1; n < 100; n++)
            {
                auto const root = n == 1 ? std::string{ "/midi" } : "/midi" + std::to_string(n);

                if (std::none_of(settings.Rows.begin(), settings.Rows.end(),
                    [&root](patchbay::OscRow const& row) { return row.Root == root && row.Format == patchbay::OscFormat::Paths; }))
                {
                    return root;
                }
            }

            return "/midi";
        }

        std::optional<std::string> AsciiRoot(_In_ winrt::hstring const& text)
        {
            std::string root{};

            for (auto const c : text)
            {
                if (c >= 0x80)
                {
                    return std::nullopt;
                }

                root += static_cast<char>(c);
            }

            // A trailing slash is easy to type and means nothing.
            while (root.size() > 1 && root.back() == '/')
            {
                root.pop_back();
            }

            return patchbay::osc::IsValidRoot(root) ? std::optional<std::string>{ root } : std::nullopt;
        }

        // The text as ASCII, with spaces at the ends left off, or nothing when it has any other
        // characters, which OSC addresses never do.
        std::optional<std::string> AsciiText(_In_ winrt::hstring const& text)
        {
            std::string ascii{};

            for (auto const c : text)
            {
                if (c >= 0x80)
                {
                    return std::nullopt;
                }

                ascii += static_cast<char>(c);
            }

            auto const first = ascii.find_first_not_of(' ');
            auto const last = ascii.find_last_not_of(' ');

            return first == std::string::npos ? std::string{} : ascii.substr(first, last - first + 1);
        }

        std::vector<std::wstring> SplitAddresses(_In_ winrt::hstring const& text)
        {
            std::vector<std::wstring> addresses{};
            std::wstring current{};

            for (auto const c : std::wstring_view{ text })
            {
                if (c == L',' || c == L';' || c == L' ' || c == L'\t' || c == L'\r' || c == L'\n')
                {
                    if (!current.empty())
                    {
                        addresses.push_back(std::move(current));
                        current.clear();
                    }

                    continue;
                }

                current += c;
            }

            if (!current.empty())
            {
                addresses.push_back(std::move(current));
            }

            return addresses;
        }

        std::wstring JoinAddresses(_In_ std::vector<std::wstring> const& addresses)
        {
            std::wstring text{};

            for (auto const& address : addresses)
            {
                text += text.empty() ? L"" : L", ";
                text += address;
            }

            return text;
        }

        controls::Grid TwoColumns(_In_ xaml::FrameworkElement const& left, _In_ xaml::FrameworkElement const& right)
        {
            controls::Grid grid{};
            grid.ColumnSpacing(10);

            for (int i = 0; i < 2; i++)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                grid.ColumnDefinitions().Append(column);
            }

            controls::Grid::SetColumn(left, 0);
            controls::Grid::SetColumn(right, 1);
            grid.Children().Append(left);
            grid.Children().Append(right);

            return grid;
        }

        bool Checked(_In_ controls::CheckBox const& box) noexcept
        {
            auto const value = box.IsChecked();
            return value && value.Value();
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildOscInspector(patchbay::PatchEndpoint const& endpoint) noexcept
    {
        try
        {
            if (!endpoint.IsOsc())
            {
                return;
            }

            auto const endpointId = endpoint.Id;
            auto const& settings = *endpoint.Osc;
            auto weak = get_weak();
            auto const content = InspectorContent();

            m_oscInspectedId = endpointId;
            m_oscHeardSignature.clear();

            // Changes one setting, keeping the inspector as it is unless asked.
            auto const change = [weak, endpointId](std::function<void(patchbay::OscConnectionSettings&)> edit, bool rebuild)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ChangeOscConnection(endpointId, edit, rebuild);
                    }
                };

            // --------------------------------------------------------------- identity and status
            {
                controls::StackPanel body{};
                body.Spacing(8);

                controls::TextBox nameBox{};
                nameBox.Header(winrt::box_value(resources::GetString(L"OscNameHeader")));
                nameBox.Text(winrt::hstring{ endpoint.DisplayName });
                nameBox.MaxLength(static_cast<int32_t>(patchbay::MaximumStringLength));

                nameBox.LostFocus([weak, endpointId, nameBox](auto&&, auto&&)
                    {
                        auto strong = weak.get();
                        auto* patch = strong == nullptr ? nullptr : strong->CurrentPatch();
                        auto* target = patch == nullptr ? nullptr : patch->FindEndpoint(endpointId);
                        auto const name = patchbay::SanitizeStoredString(std::wstring{ nameBox.Text() });

                        if (target == nullptr || name.empty() || name == target->DisplayName)
                        {
                            return;
                        }

                        target->DisplayName = name;
                        strong->InspectorTitle().Text(winrt::hstring{ name });
                        strong->CommitChange(true, false);
                        strong->RebuildCanvas();
                    });

                body.Children().Append(nameBox);

                m_oscStatusText = Line({}, 12, L"TextFillColorSecondaryBrush");
                xaml::Automation::AutomationProperties::SetLiveSetting(m_oscStatusText, xaml::Automation::Peers::AutomationLiveSetting::Polite);
                body.Children().Append(m_oscStatusText);

                content.Children().Append(parts::Card(body));
            }

            auto const transport = settings.Transport;

            // --------------------------------------------------------------- network
            {
                auto section = Section(resources::GetString(L"OscTransportHeading"));

                auto transportBox = Choices(resources::GetString(L"OscTransportHeader"), TransportNames(), static_cast<int32_t>(transport));
                transportBox.SelectionChanged([change, transportBox](auto&&, auto&&)
                    {
                        auto const value = TransportFrom(transportBox);
                        change([value](patchbay::OscConnectionSettings& s) { s.Transport = value; }, true);
                    });

                auto framingBox = Choices(resources::GetString(L"OscTcpFramingHeader"), FramingNames(),
                    settings.TcpFraming == patchbay::OscTcpFraming::SizeFirst ? 1 : 0);
                framingBox.IsEnabled(transport != patchbay::OscTransport::Udp);
                framingBox.SelectionChanged([change, framingBox](auto&&, auto&&)
                    {
                        auto const value = framingBox.SelectedIndex() == 1 ? patchbay::OscTcpFraming::SizeFirst : patchbay::OscTcpFraming::Slip;
                        change([value](patchbay::OscConnectionSettings& s) { s.TcpFraming = value; }, false);
                    });

                section.Children().Append(TwoColumns(transportBox, framingBox));
                section.Children().Append(parts::HeadingHint(TransportHint(transport)));
                content.Children().Append(section);
            }

            // --------------------------------------------------------------- receiving
            if (transport != patchbay::OscTransport::TcpConnect)
            {
                auto section = Section(resources::GetString(L"OscReceiveHeading"));

                auto port = Port(resources::GetString(L"OscReceivePortHeader"), settings.ReceivePort);
                port.ValueChanged([change](controls::NumberBox const& box, auto&&)
                    {
                        auto const value = PortFrom(box);
                        change([value](patchbay::OscConnectionSettings& s) { s.ReceivePort = value; }, false);
                    });

                auto listen = Choices(resources::GetString(L"OscListenOnHeader"),
                    { resources::GetString(L"OscListenEveryNetwork"), resources::GetString(L"OscListenThisPc") },
                    settings.ListenOn == patchbay::OscListenOn::ThisPc ? 1 : 0);
                listen.SelectionChanged([change, listen](auto&&, auto&&)
                    {
                        auto const value = listen.SelectedIndex() == 1 ? patchbay::OscListenOn::ThisPc : patchbay::OscListenOn::EveryNetwork;
                        change([value](patchbay::OscConnectionSettings& s) { s.ListenOn = value; }, false);
                    });

                section.Children().Append(TwoColumns(port, listen));
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscReceivePortHint")));

                auto advertise = parts::Check(resources::GetString(L"OscAdvertise"), settings.Advertise);
                advertise.Click([change, advertise](auto&&, auto&&)
                    {
                        auto const value = Checked(advertise);
                        change([value](patchbay::OscConnectionSettings& s) { s.Advertise = value; }, false);
                    });
                section.Children().Append(advertise);
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscAdvertiseHint")));

                // Waiting for apps over TCP, there's no computer it sends to, so that choice isn't there.
                auto const tcpListen = transport == patchbay::OscTransport::TcpListen;

                auto accept = tcpListen
                    ? Choices(resources::GetString(L"OscAcceptHeader"),
                        { resources::GetString(L"OscAcceptAnyone"), resources::GetString(L"OscAcceptList") },
                        settings.AcceptFrom == patchbay::OscAcceptFrom::List ? 1 : 0)
                    : Choices(resources::GetString(L"OscAcceptHeader"),
                        { resources::GetString(L"OscAcceptAnyone"), resources::GetString(L"OscAcceptSendHost"), resources::GetString(L"OscAcceptList") },
                        static_cast<int32_t>(settings.AcceptFrom));
                accept.SelectionChanged([change, accept, tcpListen](auto&&, auto&&)
                    {
                        auto const index = accept.SelectedIndex();
                        auto const value = tcpListen
                            ? (index == 1 ? patchbay::OscAcceptFrom::List : patchbay::OscAcceptFrom::Anyone)
                            : static_cast<patchbay::OscAcceptFrom>(std::clamp(index, 0, 2));
                        change([value](patchbay::OscConnectionSettings& s) { s.AcceptFrom = value; }, true);
                    });
                section.Children().Append(accept);

                if (settings.AcceptFrom == patchbay::OscAcceptFrom::List)
                {
                    controls::TextBox list{};
                    list.Header(winrt::box_value(resources::GetString(L"OscAcceptListHeader")));
                    list.PlaceholderText(resources::GetString(L"OscAcceptListPlaceholder"));
                    list.Text(winrt::hstring{ JoinAddresses(settings.AcceptAddresses) });
                    list.LostFocus([change, list](auto&&, auto&&)
                        {
                            auto const addresses = SplitAddresses(list.Text());
                            change([addresses](patchbay::OscConnectionSettings& s) { s.AcceptAddresses = addresses; }, false);
                        });
                    section.Children().Append(list);
                }

                section.Children().Append(parts::HeadingHint(resources::GetString(
                    settings.AcceptFrom == patchbay::OscAcceptFrom::Anyone ? L"OscAcceptAnyoneHint" :
                    settings.AcceptFrom == patchbay::OscAcceptFrom::SendHost ? L"OscAcceptSendHostHint" :
                    L"OscAcceptListHint")));

                auto rate = parts::NumberBox(resources::GetString(L"OscRateLimitHeader"),
                    patchbay::MinimumOscReceiveRateLimit, patchbay::MaximumOscReceiveRateLimit, settings.ReceiveRateLimit, 1000);
                rate.ValueChanged([change](controls::NumberBox const& box, auto&&)
                    {
                        if (!std::isfinite(box.Value()))
                        {
                            return;
                        }

                        auto const value = static_cast<uint32_t>(box.Value());
                        change([value](patchbay::OscConnectionSettings& s) { s.ReceiveRateLimit = value; }, false);
                    });
                section.Children().Append(rate);

                midiapp::UseTouchSpinButtons({ port, rate });
                content.Children().Append(section);
            }

            // --------------------------------------------------------------- sending
            if (transport == patchbay::OscTransport::TcpListen)
            {
                auto section = Section(resources::GetString(L"OscSendHeading"));
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscTcpListenSendHint")));
                content.Children().Append(section);
            }
            else
            {
                auto section = Section(resources::GetString(L"OscSendHeading"));

                controls::TextBox host{};
                host.Header(winrt::box_value(resources::GetString(L"OscSendHostHeader")));
                host.PlaceholderText(resources::GetString(L"OscSendHostPlaceholder"));
                host.Text(winrt::hstring{ settings.SendHost });
                host.MaxLength(static_cast<int32_t>(patchbay::MaximumOscHostLength));
                host.LostFocus([change, host](auto&&, auto&&)
                    {
                        auto const value = patchbay::SanitizeStoredString(std::wstring{ host.Text() });
                        change([value](patchbay::OscConnectionSettings& s) { s.SendHost = value; }, false);
                    });

                auto port = Port(resources::GetString(L"OscSendPortHeader"), settings.SendPort);
                port.ValueChanged([change](controls::NumberBox const& box, auto&&)
                    {
                        auto const value = PortFrom(box);
                        change([value](patchbay::OscConnectionSettings& s) { s.SendPort = value; }, false);
                    });

                section.Children().Append(TwoColumns(host, port));
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscSendHostHint")));

                section.Children().Append(FindOnNetwork(
                    [transport]() { return ServiceTypeOf(transport); },
                    [change](patchbay::OscFoundApp const& app)
                    {
                        auto const found = app;
                        change([found](patchbay::OscConnectionSettings& s)
                            {
                                s.SendHost = found.Host;
                                s.SendPort = found.Port;
                            }, true);
                    }));

                // Over TCP, replies come back on the connection it made.
                if (transport == patchbay::OscTransport::Udp)
                {
                    auto lastSender = parts::Check(resources::GetString(L"OscSendToLastSender"), settings.SendToLastSender);
                    lastSender.Click([change, lastSender](auto&&, auto&&)
                        {
                            auto const value = Checked(lastSender);
                            change([value](patchbay::OscConnectionSettings& s) { s.SendToLastSender = value; }, false);
                        });
                    section.Children().Append(lastSender);
                    section.Children().Append(parts::HeadingHint(resources::GetString(L"OscSendToLastSenderHint")));
                }

                controls::StackPanel testRow{};
                testRow.Orientation(controls::Orientation::Horizontal);
                testRow.Spacing(10);

                controls::Button test{};
                test.Content(winrt::box_value(resources::GetString(L"OscTestButton")));
                testRow.Children().Append(test);

                auto testResult = Line({}, 12, L"TextFillColorSecondaryBrush");
                testResult.VerticalAlignment(xaml::VerticalAlignment::Center);
                testRow.Children().Append(testResult);

                test.Click([weak, endpointId, test, testResult](auto&&, auto&&) -> winrt::fire_and_forget
                    {
                        auto strong = weak.get();
                        auto* patch = strong == nullptr ? nullptr : strong->CurrentPatch();
                        auto* target = patch == nullptr ? nullptr : patch->FindEndpoint(endpointId);

                        if (target == nullptr || !target->IsOsc())
                        {
                            co_return;
                        }

                        auto const settings = *target->Osc;

                        if (settings.SendHost.empty() || settings.SendPort == 0 || settings.Rows.empty())
                        {
                            testResult.Text(resources::GetString(L"OscTestNeedsHost"));
                            co_return;
                        }

                        test.IsEnabled(false);
                        testResult.Text(resources::GetString(L"OscTestSending"));

                        bool sent{ false };

                        co_await patchbay::RunOnBackgroundAsync([&sent, settings]()
                            {
                                sent = patchbay::OscHub::SendTestNote(settings, settings.Rows.front().Group);
                            });

                        test.IsEnabled(true);
                        testResult.Text(resources::GetString(sent ? L"OscTestSent" : L"OscTestFailed"));
                    });

                section.Children().Append(testRow);

                midiapp::UseTouchSpinButtons({ port });
                content.Children().Append(section);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the OSC connection.")

        BuildOscRowsAndTiming(endpoint);
    }
}

namespace winrt::midipatchbay::implementation
{
    _Use_decl_annotations_
    void MainWindow::ChangeOscRows(
        std::wstring const& endpointId,
        std::function<void(patchbay::PatchDocument&, patchbay::PatchEndpoint&)> const& change) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();
            auto* target = patch == nullptr ? nullptr : patch->FindEndpoint(endpointId);

            if (target == nullptr || !target->IsOsc())
            {
                return;
            }

            change(*patch, *target);

            patchbay::SanitizeOscConnection(*target->Osc);

            CommitChange(true, false);
            RebuildCanvas();
            RefreshInspector();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the OSC connection's rows.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildOscRowsAndTiming(patchbay::PatchEndpoint const& endpoint) noexcept
    {
        try
        {
            if (!endpoint.IsOsc())
            {
                return;
            }

            auto const endpointId = endpoint.Id;
            auto const& settings = *endpoint.Osc;
            auto weak = get_weak();
            auto const content = InspectorContent();

            // Changes one row, found by its group. A change that adds or takes away settings
            // builds the inspector again.
            auto const changeRow = [weak, endpointId](uint8_t group, std::function<void(patchbay::OscRow&)> edit, bool rebuild)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ChangeOscConnection(endpointId, [group, edit](patchbay::OscConnectionSettings& s)
                            {
                                for (auto& row : s.Rows)
                                {
                                    if (row.Group == group)
                                    {
                                        edit(row);
                                    }
                                }
                            }, rebuild);
                    }
                };

            // --------------------------------------------------------------- rows
            {
                auto section = Section(resources::GetString(L"OscRowsHeading"));
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscRowsHint")));

                for (auto const& row : settings.Rows)
                {
                    auto const group = row.Group;

                    controls::StackPanel body{};
                    body.Spacing(8);

                    auto title = parts::Heading(patchbay::DescribeGroupIndex(group, patchbay::OscRowLabel(row)));
                    title.TextWrapping(xaml::TextWrapping::Wrap);
                    body.Children().Append(title);

                    // Presets, then "custom" when the row matches none of them.
                    auto names = PresetNames();
                    auto const preset = patchbay::OscPresetOf(row);

                    if (!preset.has_value())
                    {
                        names.push_back(resources::GetString(L"OscPresetCustom"));
                    }

                    auto presetBox = Choices(resources::GetString(L"OscPresetHeader"), names,
                        preset.has_value() ? static_cast<int32_t>(*preset) : static_cast<int32_t>(names.size() - 1));

                    presetBox.SelectionChanged([changeRow, group, presetBox](auto&&, auto&&)
                        {
                            auto const index = presetBox.SelectedIndex();

                            if (index < 0 || index >= patchbay::OscPresetCount)
                            {
                                return;
                            }

                            changeRow(group, [index](patchbay::OscRow& r) { r = WithPreset(r, static_cast<patchbay::OscPreset>(index)); }, true);
                        });

                    body.Children().Append(presetBox);

                    auto const isCustom = row.Format == patchbay::OscFormat::Custom;

                    // The address its messages go under. Rules carry their own.
                    if (!isCustom)
                    {
                        controls::TextBox root{};
                        root.Header(winrt::box_value(resources::GetString(L"OscRootHeader")));
                        root.PlaceholderText(resources::GetString(L"OscRootPlaceholder"));
                        root.Text(winrt::hstring{ std::wstring{ row.Root.begin(), row.Root.end() } });

                        auto rootError = Line(resources::GetString(L"OscRootInvalid"), 11, L"SystemFillColorCriticalBrush");
                        rootError.Visibility(xaml::Visibility::Collapsed);

                        root.LostFocus([changeRow, group, root, rootError](auto&&, auto&&)
                            {
                                auto const value = AsciiRoot(root.Text());

                                rootError.Visibility(value.has_value() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

                                if (value.has_value())
                                {
                                    changeRow(group, [value](patchbay::OscRow& r) { r.Root = *value; }, true);
                                }
                            });

                        body.Children().Append(root);
                        body.Children().Append(rootError);
                    }

                    auto format = Choices(resources::GetString(L"OscFormatHeader"),
                        { patchbay::OscFormatName(patchbay::OscFormat::Paths), patchbay::OscFormatName(patchbay::OscFormat::MidiType),
                          patchbay::OscFormatName(patchbay::OscFormat::Arguments), patchbay::OscFormatName(patchbay::OscFormat::Custom) },
                        static_cast<int32_t>(row.Format));

                    format.SelectionChanged([changeRow, group, format](auto&&, auto&&)
                        {
                            auto const value = static_cast<patchbay::OscFormat>(std::clamp(format.SelectedIndex(), 0, patchbay::OscFormatCount - 1));
                            changeRow(group, [value](patchbay::OscRow& r) { r.Format = value; }, true);
                        });

                    body.Children().Append(format);
                    body.Children().Append(parts::HeadingHint(resources::GetString(
                        row.Format == patchbay::OscFormat::Paths ? L"OscFormatPathsHint" :
                        row.Format == patchbay::OscFormat::MidiType ? L"OscFormatMidiTypeHint" :
                        row.Format == patchbay::OscFormat::Custom ? L"OscFormatCustomHint" :
                        L"OscFormatArgumentsHint")));

                    if (row.Format == patchbay::OscFormat::Arguments)
                    {
                        auto namesBox = Choices(resources::GetString(L"OscNamesHeader"),
                            { resources::GetString(L"OscNamesOpenStageControl"), resources::GetString(L"OscNamesOscMidiOsc") },
                            static_cast<int32_t>(row.Names));

                        namesBox.SelectionChanged([changeRow, group, namesBox](auto&&, auto&&)
                            {
                                auto const value = namesBox.SelectedIndex() == 1 ? patchbay::OscArgumentNames::OscMidiOsc : patchbay::OscArgumentNames::OpenStageControl;
                                changeRow(group, [value](patchbay::OscRow& r) { r.Names = value; }, true);
                            });

                        body.Children().Append(namesBox);
                    }

                    if (isCustom)
                    {
                        for (size_t i = 0; i < row.Rules.size(); i++)
                        {
                            auto const& rule = row.Rules[i];

                            // Changes one rule, if it's still there.
                            auto const changeRule = [changeRow, group, i](auto edit, bool rebuildAfter)
                                {
                                    changeRow(group, [i, edit](patchbay::OscRow& r)
                                        {
                                            if (i < r.Rules.size())
                                            {
                                                edit(r.Rules[i]);
                                            }
                                        }, rebuildAfter);
                                };

                            controls::StackPanel ruleBody{};
                            ruleBody.Spacing(6);
                            ruleBody.Padding(xaml::ThicknessHelper::FromLengths(12, 8, 0, 8));
                            ruleBody.BorderThickness(xaml::ThicknessHelper::FromLengths(2, 0, 0, 0));
                            ruleBody.BorderBrush(BrushOf(L"ControlStrokeColorDefaultBrush"));

                            ruleBody.Children().Append(Line(resources::FormatString(L"OscRuleHeadingFormat", static_cast<uint32_t>(i + 1)), 13));

                            controls::TextBox address{};
                            address.Header(winrt::box_value(resources::GetString(L"OscRuleAddressHeader")));
                            address.PlaceholderText(resources::GetString(L"OscRuleAddressPlaceholder"));
                            address.Text(winrt::hstring{ std::wstring{ rule.Address.begin(), rule.Address.end() } });

                            auto addressError = Line(resources::GetString(L"OscRuleAddressInvalid"), 11, L"SystemFillColorCriticalBrush");
                            addressError.TextWrapping(xaml::TextWrapping::Wrap);
                            addressError.Visibility(xaml::Visibility::Collapsed);

                            address.LostFocus([changeRule, address, addressError](auto&&, auto&&)
                                {
                                    auto const text = AsciiText(address.Text());
                                    auto const valid = text.has_value() && patchbay::osc::IsValidRuleAddress(*text);

                                    addressError.Visibility(valid ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

                                    if (valid)
                                    {
                                        changeRule([value = *text](patchbay::OscRule& r) { r.Address = value; }, true);
                                    }
                                });

                            ruleBody.Children().Append(address);
                            ruleBody.Children().Append(addressError);

                            auto message = Choices(resources::GetString(L"OscRuleMessageHeader"),
                                { resources::GetString(L"OscRuleMessageControlChange"), resources::GetString(L"OscRuleMessageNote"),
                                  resources::GetString(L"OscRuleMessageProgramChange"), resources::GetString(L"OscRuleMessagePitchBend"),
                                  resources::GetString(L"OscRuleMessageChannelPressure"), resources::GetString(L"OscRuleMessagePolyPressure") },
                                static_cast<int32_t>(rule.Message));

                            message.SelectionChanged([changeRule, message](auto&&, auto&&)
                                {
                                    auto const value = static_cast<patchbay::OscRuleMessage>(std::clamp(message.SelectedIndex(), 0, patchbay::OscRuleMessageCount - 1));
                                    changeRule([value](patchbay::OscRule& r) { r.Message = value; }, true);
                                });

                            auto direction = Choices(resources::GetString(L"OscRuleDirectionHeader"),
                                { resources::GetString(L"OscRuleDirectionBoth"), resources::GetString(L"OscRuleDirectionIn"), resources::GetString(L"OscRuleDirectionOut") },
                                static_cast<int32_t>(rule.Direction));

                            direction.SelectionChanged([changeRule, direction](auto&&, auto&&)
                                {
                                    auto const value = static_cast<patchbay::OscRuleDirection>(std::clamp(direction.SelectedIndex(), 0, 2));
                                    changeRule([value](patchbay::OscRule& r) { r.Direction = value; }, true);
                                });

                            ruleBody.Children().Append(TwoColumns(message, direction));

                            // A number, or a part with an offset. People count channels from 1.
                            auto const numberBox = [](winrt::hstring const& header, patchbay::OscRuleNumber const& value, int32_t shown)
                                {
                                    controls::TextBox box{};
                                    box.Header(winrt::box_value(header));
                                    box.Text(winrt::hstring{ patchbay::osc::RuleNumberText(value, shown) });
                                    return box;
                                };

                            auto const numberError = Line(resources::GetString(L"OscRuleNumberInvalid"), 11, L"SystemFillColorCriticalBrush");
                            numberError.TextWrapping(xaml::TextWrapping::Wrap);
                            numberError.Visibility(xaml::Visibility::Collapsed);

                            auto channel = numberBox(resources::GetString(L"OscRuleChannelHeader"), rule.Channel, 1);

                            channel.LostFocus([changeRule, channel, numberError](auto&&, auto&&)
                                {
                                    auto value = patchbay::osc::ParseRuleNumber(channel.Text());
                                    auto const valid = value.has_value() && (!value->Part.empty() || (value->Offset >= 1 && value->Offset <= 16));

                                    numberError.Visibility(valid ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

                                    if (valid)
                                    {
                                        value->Offset -= 1;
                                        changeRule([number = *value](patchbay::OscRule& r) { r.Channel = number; }, true);
                                    }
                                });

                            auto const usesNumber = rule.Message != patchbay::OscRuleMessage::PitchBend && rule.Message != patchbay::OscRuleMessage::ChannelPressure;

                            if (usesNumber)
                            {
                                auto number = numberBox(resources::GetString(
                                    rule.Message == patchbay::OscRuleMessage::ControlChange ? L"OscRuleNumberController" :
                                    rule.Message == patchbay::OscRuleMessage::ProgramChange ? L"OscRuleNumberProgram" :
                                    L"OscRuleNumberNote"), rule.Number, 0);

                                number.LostFocus([changeRule, number, numberError](auto&&, auto&&)
                                    {
                                        auto const value = patchbay::osc::ParseRuleNumber(number.Text());
                                        auto const valid = value.has_value() && (!value->Part.empty() || (value->Offset >= 0 && value->Offset <= 127));

                                        numberError.Visibility(valid ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

                                        if (valid)
                                        {
                                            changeRule([n = *value](patchbay::OscRule& r) { r.Number = n; }, true);
                                        }
                                    });

                                ruleBody.Children().Append(TwoColumns(channel, number));
                            }
                            else
                            {
                                ruleBody.Children().Append(channel);
                            }

                            ruleBody.Children().Append(numberError);
                            ruleBody.Children().Append(parts::HeadingHint(resources::GetString(L"OscRuleNumberHint")));

                            auto const valueBox = [](winrt::hstring const& header, double value)
                                {
                                    auto box = parts::NumberBox(header, -patchbay::MaximumOscRuleValue, patchbay::MaximumOscRuleValue, value, 10);
                                    box.SmallChange(0.1);
                                    return box;
                                };

                            auto low = valueBox(resources::GetString(L"OscRuleLowHeader"), rule.Low);
                            auto high = valueBox(resources::GetString(L"OscRuleHighHeader"), rule.High);

                            // Both ends the same would be no range at all, so that change isn't kept.
                            low.ValueChanged([changeRule](controls::NumberBox const& box, auto&&)
                                {
                                    auto const value = box.Value();

                                    if (std::isfinite(value))
                                    {
                                        changeRule([value](patchbay::OscRule& r) { if (value != r.High) { r.Low = value; } }, false);
                                    }
                                });

                            high.ValueChanged([changeRule](controls::NumberBox const& box, auto&&)
                                {
                                    auto const value = box.Value();

                                    if (std::isfinite(value))
                                    {
                                        changeRule([value](patchbay::OscRule& r) { if (value != r.Low) { r.High = value; } }, false);
                                    }
                                });

                            ruleBody.Children().Append(TwoColumns(low, high));
                            ruleBody.Children().Append(parts::HeadingHint(resources::GetString(L"OscRuleRangeHint")));

                            auto argument = parts::NumberBox(resources::GetString(L"OscRuleArgumentHeader"), 1, patchbay::MaximumOscRuleArgument + 1.0, rule.Argument + 1.0, 1);

                            argument.ValueChanged([changeRule](controls::NumberBox const& box, auto&&)
                                {
                                    if (std::isfinite(box.Value()))
                                    {
                                        auto const value = static_cast<uint8_t>(std::clamp(box.Value() - 1, 0.0, static_cast<double>(patchbay::MaximumOscRuleArgument)));
                                        changeRule([value](patchbay::OscRule& r) { r.Argument = value; }, false);
                                    }
                                });

                            controls::Button removeRule{};
                            removeRule.Content(winrt::box_value(resources::GetString(L"OscRuleRemove")));
                            removeRule.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                            removeRule.Click([changeRow, group, i](auto&&, auto&&)
                                {
                                    changeRow(group, [i](patchbay::OscRow& r)
                                        {
                                            if (i < r.Rules.size())
                                            {
                                                r.Rules.erase(r.Rules.begin() + static_cast<ptrdiff_t>(i));
                                            }
                                        }, true);
                                });

                            ruleBody.Children().Append(TwoColumns(argument, removeRule));
                            midiapp::UseTouchSpinButtons({ low, high, argument });

                            // Say so when a rule can't do what it's set to.
                            if (rule.Direction != patchbay::OscRuleDirection::Out && !patchbay::osc::RuleReceives(rule))
                            {
                                auto warning = Line(resources::GetString(L"OscRuleCantReceive"), 11, L"SystemFillColorCautionBrush");
                                warning.TextWrapping(xaml::TextWrapping::Wrap);
                                ruleBody.Children().Append(warning);
                            }

                            if (rule.Direction != patchbay::OscRuleDirection::In && !patchbay::osc::RuleSends(rule))
                            {
                                auto warning = Line(resources::GetString(L"OscRuleCantSend"), 11, L"SystemFillColorCautionBrush");
                                warning.TextWrapping(xaml::TextWrapping::Wrap);
                                ruleBody.Children().Append(warning);
                            }

                            body.Children().Append(ruleBody);
                        }

                        controls::Button addRule{};
                        addRule.Content(winrt::box_value(resources::GetString(L"OscAddRule")));
                        addRule.IsEnabled(row.Rules.size() < patchbay::MaximumOscRules);

                        addRule.Click([changeRow, group](auto&&, auto&&)
                            {
                                changeRow(group, [](patchbay::OscRow& r)
                                    {
                                        if (r.Rules.size() < patchbay::MaximumOscRules)
                                        {
                                            patchbay::OscRule rule{};
                                            rule.Address = "/control{n}";
                                            rule.Number = patchbay::OscRuleNumber{ "n", 0 };
                                            r.Rules.push_back(rule);
                                        }
                                    }, true);
                            });

                        body.Children().Append(addRule);
                    }

                    // The MIDI type carries channels in its status byte, so none of these apply.
                    // Rules say their own.
                    if (row.Format != patchbay::OscFormat::MidiType && !isCustom)
                    {
                        auto channels = Choices(resources::GetString(L"OscChannelsHeader"),
                            { resources::GetString(L"OscChannelsFromOne"), resources::GetString(L"OscChannelsFromZero") },
                            row.ChannelsFromZero ? 1 : 0);

                        channels.SelectionChanged([changeRow, group, channels](auto&&, auto&&)
                            {
                                auto const value = channels.SelectedIndex() == 1;
                                changeRow(group, [value](patchbay::OscRow& r) { r.ChannelsFromZero = value; }, true);
                            });

                        auto values = Choices(resources::GetString(L"OscValuesHeader"),
                            { resources::GetString(L"OscValuesIntegers"), resources::GetString(L"OscValuesFloats"), resources::GetString(L"OscValuesUnit") },
                            static_cast<int32_t>(row.Values));

                        values.SelectionChanged([changeRow, group, values](auto&&, auto&&)
                            {
                                auto const value = static_cast<patchbay::OscValueKind>(std::clamp(values.SelectedIndex(), 0, 2));
                                changeRow(group, [value](patchbay::OscRow& r) { r.Values = value; }, false);
                            });

                        body.Children().Append(TwoColumns(channels, values));

                        auto bend = Choices(resources::GetString(L"OscPitchBendHeader"),
                            { resources::GetString(L"OscPitchBendUnsigned"), resources::GetString(L"OscPitchBendSigned") },
                            static_cast<int32_t>(row.PitchBend));

                        bend.SelectionChanged([changeRow, group, bend](auto&&, auto&&)
                            {
                                auto const value = bend.SelectedIndex() == 1 ? patchbay::OscPitchBendRange::Signed : patchbay::OscPitchBendRange::Unsigned;
                                changeRow(group, [value](patchbay::OscRow& r) { r.PitchBend = value; }, false);
                            });

                        auto noteOff = Choices(resources::GetString(L"OscNoteOffHeader"),
                            { resources::GetString(L"OscNoteOffVelocityZero"), resources::GetString(L"OscNoteOffOwnAddress") },
                            static_cast<int32_t>(row.NoteOff));

                        noteOff.SelectionChanged([changeRow, group, noteOff](auto&&, auto&&)
                            {
                                auto const value = noteOff.SelectedIndex() == 1 ? patchbay::OscNoteOff::OwnAddress : patchbay::OscNoteOff::VelocityZero;
                                changeRow(group, [value](patchbay::OscRow& r) { r.NoteOff = value; }, true);
                            });

                        body.Children().Append(TwoColumns(bend, noteOff));

                        // Paths without a channel, and OSC_MIDI_OSC's names, which never have one.
                        auto const first = row.ChannelsFromZero ? 0.0 : 1.0;
                        auto channel = parts::NumberBox(resources::GetString(L"OscChannelHeader"), first, first + 15, row.Channel + first, 4);

                        channel.ValueChanged([changeRow, group, first](controls::NumberBox const& box, auto&&)
                            {
                                if (!std::isfinite(box.Value()))
                                {
                                    return;
                                }

                                auto const value = static_cast<uint8_t>(std::clamp(box.Value() - first, 0.0, 15.0));
                                changeRow(group, [value](patchbay::OscRow& r) { r.Channel = value; }, false);
                            });

                        body.Children().Append(channel);
                        body.Children().Append(parts::HeadingHint(resources::GetString(L"OscChannelHint")));
                        midiapp::UseTouchSpinButtons({ channel });
                    }

                    // Rules carry channel messages only.
                    if (!isCustom)
                    {
                        auto sysex = parts::Check(resources::GetString(L"OscSendSysEx"), row.SendSysEx);
                        sysex.Click([changeRow, group, sysex](auto&&, auto&&)
                            {
                                auto const value = Checked(sysex);
                                changeRow(group, [value](patchbay::OscRow& r) { r.SendSysEx = value; }, false);
                            });
                        body.Children().Append(sysex);

                        auto clock = parts::Check(resources::GetString(L"OscSendClock"), row.SendClock);
                        clock.Click([changeRow, group, clock](auto&&, auto&&)
                            {
                                auto const value = Checked(clock);
                                changeRow(group, [value](patchbay::OscRow& r) { r.SendClock = value; }, false);
                            });
                        body.Children().Append(clock);
                    }

                    auto midi2 = parts::Check(resources::GetString(L"OscMidi2"), row.Midi2);
                    midi2.Click([changeRow, group, midi2](auto&&, auto&&)
                        {
                            auto const value = Checked(midi2);
                            changeRow(group, [value](patchbay::OscRow& r) { r.Midi2 = value; }, false);
                        });
                    body.Children().Append(midi2);
                    body.Children().Append(parts::HeadingHint(resources::GetString(L"OscMidi2Hint")));

                    // Which group the row is. Moving it takes its links along.
                    std::vector<winrt::hstring> groupNames{};
                    std::vector<uint8_t> groupValues{};
                    int32_t selectedGroup{ 0 };

                    for (uint8_t g = 0; g < 16; g++)
                    {
                        if (g != group && patchbay::OscRowOnGroup(settings, g) != nullptr)
                        {
                            continue;
                        }

                        if (g == group)
                        {
                            selectedGroup = static_cast<int32_t>(groupValues.size());
                        }

                        groupNames.push_back(patchbay::DescribeGroupIndex(g, {}));
                        groupValues.push_back(g);
                    }

                    auto groupBox = Choices(resources::GetString(L"OscGroupHeader"), groupNames, selectedGroup);

                    groupBox.SelectionChanged([weak, endpointId, group, groupBox, groupValues](auto&&, auto&&)
                        {
                            auto const index = groupBox.SelectedIndex();
                            auto strong = weak.get();

                            if (strong == nullptr || index < 0 || static_cast<size_t>(index) >= groupValues.size() || groupValues[index] == group)
                            {
                                return;
                            }

                            auto const moved = groupValues[index];

                            strong->ChangeOscRows(endpointId, [endpointId, group, moved](patchbay::PatchDocument& patch, patchbay::PatchEndpoint& target)
                                {
                                    for (auto& r : target.Osc->Rows)
                                    {
                                        if (r.Group == group)
                                        {
                                            r.Group = moved;
                                        }
                                    }

                                    for (auto& link : patch.Connections)
                                    {
                                        if (link.SourceId == endpointId && link.SourceGroupIndex == group)
                                        {
                                            link.SourceGroupIndex = moved;
                                        }

                                        if (link.DestinationId == endpointId && link.DestinationGroupIndex == group)
                                        {
                                            link.DestinationGroupIndex = moved;
                                        }
                                    }
                                });
                        });

                    controls::Button remove{};
                    remove.Content(winrt::box_value(resources::GetString(L"OscRemoveRow")));
                    remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                    remove.Click([weak, endpointId, group](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                strong->ChangeOscRows(endpointId, [endpointId, group](patchbay::PatchDocument& patch, patchbay::PatchEndpoint& target)
                                    {
                                        std::erase_if(target.Osc->Rows, [group](patchbay::OscRow const& r) { return r.Group == group; });

                                        // A link to a row that's gone would carry nothing.
                                        std::erase_if(patch.Connections, [&endpointId, group](patchbay::PatchConnection const& link)
                                            {
                                                return (link.SourceId == endpointId && link.SourceGroupIndex == group) ||
                                                    (link.DestinationId == endpointId && link.DestinationGroupIndex == group);
                                            });
                                    });
                            }
                        });

                    body.Children().Append(TwoColumns(groupBox, remove));

                    section.Children().Append(parts::Card(body));
                }

                controls::Button add{};
                add.Content(winrt::box_value(resources::GetString(L"OscAddRow")));
                add.IsEnabled(patchbay::FreeOscGroup(settings).has_value());

                add.Click([weak, endpointId](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ChangeOscRows(endpointId, [](patchbay::PatchDocument&, patchbay::PatchEndpoint& target)
                                {
                                    auto const group = patchbay::FreeOscGroup(*target.Osc);

                                    if (!group.has_value())
                                    {
                                        return;
                                    }

                                    auto row = patchbay::OscRowFromPreset(patchbay::OscPreset::Paths, *group);
                                    row.Root = FreeRoot(*target.Osc);
                                    target.Osc->Rows.push_back(row);
                                });
                        }
                    });

                section.Children().Append(add);
                content.Children().Append(section);
            }

            // --------------------------------------------------------------- timing
            {
                auto section = Section(resources::GetString(L"OscTimingHeading"));

                auto received = Choices(resources::GetString(L"OscReceivedTimeTagsHeader"),
                    { resources::GetString(L"OscTimeTagsPlayOnArrival"), resources::GetString(L"OscTimeTagsSenderTime"),
                      resources::GetString(L"OscTimeTagsKeepSpacing") },
                    static_cast<int32_t>(settings.ReceivedTimeTags));

                received.SelectionChanged([weak, endpointId, received](auto&&, auto&&)
                    {
                        auto const value = static_cast<patchbay::OscReceivedTimeTags>(std::clamp(received.SelectedIndex(), 0, 2));

                        if (auto strong = weak.get())
                        {
                            strong->ChangeOscConnection(endpointId, [value](patchbay::OscConnectionSettings& s) { s.ReceivedTimeTags = value; }, true);
                        }
                    });

                section.Children().Append(received);
                section.Children().Append(parts::HeadingHint(resources::GetString(
                    settings.ReceivedTimeTags == patchbay::OscReceivedTimeTags::PlayOnArrival ? L"OscTimeTagsPlayOnArrivalHint" :
                    settings.ReceivedTimeTags == patchbay::OscReceivedTimeTags::UseSenderTime ? L"OscTimeTagsSenderTimeHint" :
                    L"OscTimeTagsKeepSpacingHint")));

                if (settings.ReceivedTimeTags == patchbay::OscReceivedTimeTags::KeepSenderSpacing)
                {
                    auto delay = parts::NumberBox(resources::GetString(L"OscSpacingDelayHeader"), 0,
                        patchbay::MaximumOscSpacingDelayMilliseconds, settings.SpacingDelayMilliseconds, 10);

                    delay.ValueChanged([weak, endpointId](controls::NumberBox const& box, auto&&)
                        {
                            if (!std::isfinite(box.Value()))
                            {
                                return;
                            }

                            auto const value = static_cast<uint32_t>(box.Value());

                            if (auto strong = weak.get())
                            {
                                strong->ChangeOscConnection(endpointId, [value](patchbay::OscConnectionSettings& s) { s.SpacingDelayMilliseconds = value; }, false);
                            }
                        });

                    section.Children().Append(delay);
                    midiapp::UseTouchSpinButtons({ delay });
                }

                auto sending = Choices(resources::GetString(L"OscSendTimingHeader"),
                    { resources::GetString(L"OscSendTimingAtItsTime"), resources::GetString(L"OscSendTimingEarly") },
                    static_cast<int32_t>(settings.SendTiming));

                sending.SelectionChanged([weak, endpointId, sending](auto&&, auto&&)
                    {
                        auto const value = sending.SelectedIndex() == 1 ? patchbay::OscSendTiming::EarlyWithTimeTags : patchbay::OscSendTiming::AtItsTime;

                        if (auto strong = weak.get())
                        {
                            strong->ChangeOscConnection(endpointId, [value](patchbay::OscConnectionSettings& s) { s.SendTiming = value; }, true);
                        }
                    });

                section.Children().Append(sending);
                section.Children().Append(parts::HeadingHint(resources::GetString(
                    settings.SendTiming == patchbay::OscSendTiming::AtItsTime ? L"OscSendTimingAtItsTimeHint" : L"OscSendTimingEarlyHint")));

                auto bundle = parts::Check(resources::GetString(L"OscBundleTogether"), settings.BundleTogether);
                bundle.Click([weak, endpointId, bundle](auto&&, auto&&)
                    {
                        auto const value = Checked(bundle);

                        if (auto strong = weak.get())
                        {
                            strong->ChangeOscConnection(endpointId, [value](patchbay::OscConnectionSettings& s) { s.BundleTogether = value; }, false);
                        }
                    });

                section.Children().Append(bundle);
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscBundleTogetherHint")));

                content.Children().Append(section);
            }

            // --------------------------------------------------------------- activity
            {
                auto section = Section(resources::GetString(L"OscActivityHeading"));

                m_oscCountsText = Line({}, 12, L"TextFillColorSecondaryBrush");
                section.Children().Append(m_oscCountsText);

                content.Children().Append(section);
            }

            // --------------------------------------------------------------- recently heard
            {
                auto section = Section(resources::GetString(L"OscHeardHeading"));
                section.Children().Append(parts::HeadingHint(resources::GetString(L"OscHeardHint")));

                m_oscHeardPanel = controls::StackPanel{};
                m_oscHeardPanel.Spacing(6);
                section.Children().Append(m_oscHeardPanel);

                controls::Button clear{};
                clear.Content(winrt::box_value(resources::GetString(L"OscHeardClear")));

                clear.Click([weak, endpointId](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            patchbay::OscHub::Current().ClearRecentlyHeard(patchbay::OscDeviceId(strong->m_patchKey, endpointId));
                            strong->m_oscHeardSignature.clear();
                            strong->UpdateOscStatus();
                        }
                    });

                section.Children().Append(clear);
                content.Children().Append(section);
            }

            UpdateOscStatus();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the OSC connection's rows.")
    }
}

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // "A", "A and B", or "A, B, and C"
        winrt::hstring JoinNames(_In_ std::vector<std::wstring> const& names)
        {
            if (names.size() < 2)
            {
                return names.empty() ? winrt::hstring{} : winrt::hstring{ names.front() };
            }

            if (names.size() == 2)
            {
                return resources::FormatString(L"OscNamePairFormat", names[0], names[1]);
            }

            std::wstring leading{ names.front() };

            for (size_t i = 1; i + 1 < names.size(); i++)
            {
                leading += resources::GetString(L"OscNameSeparator");
                leading += names[i];
            }

            return resources::FormatString(L"OscNameSeriesFormat", leading, names.back());
        }

        winrt::hstring NetworkTypeText(_In_ long type)
        {
            return resources::GetString(
                type == firewall::DomainNetworks ? L"OscFirewallDomainNetworkType" :
                type == firewall::PrivateNetworks ? L"OscFirewallPrivateNetworkType" :
                L"OscFirewallPublicNetworkType");
        }

        bool IsBlocked(_In_ firewall::FirewallStatus status) noexcept
        {
            return status == firewall::FirewallStatus::Closed ||
                status == firewall::FirewallStatus::ClosedByRule ||
                status == firewall::FirewallStatus::ClosedForAll;
        }

        // Each count that isn't zero, in a sentence of its own.
        std::wstring CountsText(_In_ patchbay::OscConnectionStatus const& status)
        {
            std::wstring text{ resources::FormatString(L"OscCountsFormat",
                static_cast<int64_t>(status.OscReceived), static_cast<int64_t>(status.MidiReceived),
                static_cast<int64_t>(status.MidiSent), static_cast<int64_t>(status.OscSent)) };

            auto const add = [&text](uint64_t count, wchar_t const* key)
                {
                    if (count != 0)
                    {
                        text += L' ';
                        text += resources::FormatString(key, static_cast<int64_t>(count));
                    }
                };

            add(status.Unreadable, L"OscCountUnreadableFormat");
            add(status.NoRowMatched, L"OscCountNoRowMatchedFormat");
            add(status.NotAccepted, L"OscCountNotAcceptedFormat");
            add(status.TurnedAway, L"OscCountTurnedAwayFormat");
            add(status.RateLimited, L"OscCountRateLimitedFormat");
            add(status.TooManyFromPattern, L"OscCountTooManyFormat");
            add(status.TooFarAhead, L"OscCountTooFarAheadFormat");
            add(status.NotSendable, L"OscCountNotSendableFormat");
            add(status.LeftOut, L"OscCountLeftOutFormat");
            add(status.NoRow, L"OscCountNoRowFormat");
            add(status.SendDropped, L"OscCountDroppedFormat");
            add(status.SendFailures, L"OscCountSendFailuresFormat");

            return text;
        }
    }

    void MainWindow::UpdateOscStatus() noexcept
    {
        try
        {
            if (m_oscInspectedId.empty() || m_oscStatusText == nullptr)
            {
                return;
            }

            auto const* patch = CurrentPatch();
            auto const* endpoint = patch == nullptr ? nullptr : patch->FindEndpoint(m_oscInspectedId);

            if (endpoint == nullptr || !endpoint->IsOsc())
            {
                return;
            }

            auto const endpointId = m_oscInspectedId;
            auto const status = patchbay::OscHub::Current().Status(patchbay::OscDeviceId(m_patchKey, endpointId));

            std::wstring text{ patchbay::OscListeningText(*endpoint->Osc, status) };

            if (status.has_value())
            {
                if (patchbay::OscConnectionReceives(*endpoint->Osc))
                {
                    text += L'\n';
                    text += patchbay::OscLastHeardText(*status);
                }

                if (status->HostNotFound)
                {
                    text += L'\n';
                    text += resources::FormatString(L"OscHostNotFoundFormat", endpoint->Osc->SendHost);
                }
                else if (!status->SendingTo.empty() && endpoint->Osc->Transport == patchbay::OscTransport::Udp)
                {
                    text += L'\n';
                    text += resources::FormatString(L"OscSendingToFormat", status->SendingTo);
                }
            }

            // Only when it changed, so a screen reader isn't told the same thing twice a second.
            if (m_oscStatusText.Text() != text)
            {
                m_oscStatusText.Text(winrt::hstring{ text });
            }

            if (m_oscCountsText != nullptr)
            {
                winrt::hstring const counts = status.has_value() ? winrt::hstring{ CountsText(*status) } : resources::GetString(L"OscNotRouting");

                if (m_oscCountsText.Text() != counts)
                {
                    m_oscCountsText.Text(counts);
                }
            }

            if (m_oscHeardPanel == nullptr)
            {
                return;
            }

            auto const& heard = status.has_value() ? status->RecentlyHeard : std::vector<patchbay::OscHeardMessage>{};

            auto signature = std::to_wstring(heard.size());

            if (!heard.empty())
            {
                signature += std::format(L"|{}|{}", heard.front().When, heard.front().Address);
            }

            // A rule added from the list takes its link away.
            size_t ruleCount{ 0 };

            for (auto const& row : endpoint->Osc->Rows)
            {
                ruleCount += row.Rules.size();
            }

            signature += std::format(L"|r{}", ruleCount);

            if (signature == m_oscHeardSignature)
            {
                return;
            }

            m_oscHeardSignature = signature;
            m_oscHeardPanel.Children().Clear();

            if (heard.empty())
            {
                m_oscHeardPanel.Children().Append(Line(resources::GetString(L"OscHeardNothing"), 12, L"TextFillColorTertiaryBrush"));
                return;
            }

            auto weak = get_weak();
            auto const hasFreeGroup = patchbay::FreeOscGroup(*endpoint->Osc).has_value();
            std::set<std::string> offeredRules{};

            for (auto const& message : heard)
            {
                controls::StackPanel entry{};
                entry.Spacing(2);

                auto address = Line(winrt::hstring{ message.Arguments.empty() ? message.Address : message.Address + L"  " + message.Arguments }, 12);
                address.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                address.IsTextSelectionEnabled(true);
                entry.Children().Append(address);

                auto const note = resources::FormatString(
                    !message.Accepted ? L"OscHeardKeptOutFormat" :
                    message.Matched ? L"OscHeardMatchedFormat" :
                    L"OscHeardNoRowFormat",
                    message.From);

                entry.Children().Append(Line(note, 11, message.Matched ? L"TextFillColorTertiaryBrush" : L"SystemFillColorCautionBrush"));

                if (message.Accepted && !message.Matched && message.Suggestion.has_value() && hasFreeGroup)
                {
                    controls::HyperlinkButton add{};
                    add.Content(winrt::box_value(resources::FormatString(L"OscHeardAddRowFormat", patchbay::OscRowLabel(*message.Suggestion))));
                    add.Padding(xaml::ThicknessHelper::FromLengths(0, 2, 0, 2));

                    auto const suggestion = *message.Suggestion;

                    add.Click([weak, endpointId, suggestion](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                strong->ChangeOscRows(endpointId, [suggestion](patchbay::PatchDocument&, patchbay::PatchEndpoint& target)
                                    {
                                        auto row = suggestion;

                                        // The MIDI type names its group. Anything else takes the first free one.
                                        if (row.Format != patchbay::OscFormat::MidiType || patchbay::OscRowOnGroup(*target.Osc, row.Group) != nullptr)
                                        {
                                            auto const group = patchbay::FreeOscGroup(*target.Osc);

                                            if (!group.has_value())
                                            {
                                                return;
                                            }

                                            row.Group = *group;
                                        }

                                        target.Osc->Rows.push_back(row);
                                    });
                            }
                        });

                    entry.Children().Append(add);
                }

                // An address no format takes can still have a rule. Offered once for each address,
                // and not at all when a rule has it already.
                if (message.Accepted && !message.Matched && message.RuleSuggestion.has_value())
                {
                    auto const rule = *message.RuleSuggestion;
                    auto known = !offeredRules.insert(rule.Address).second;
                    auto room = hasFreeGroup;

                    for (auto const& row : endpoint->Osc->Rows)
                    {
                        if (row.Format != patchbay::OscFormat::Custom)
                        {
                            continue;
                        }

                        room = room || row.Rules.size() < patchbay::MaximumOscRules;
                        known = known || std::any_of(row.Rules.begin(), row.Rules.end(), [&rule](patchbay::OscRule const& r) { return r.Address == rule.Address; });
                    }

                    if (!known && room)
                    {
                        controls::HyperlinkButton add{};
                        add.Content(winrt::box_value(resources::FormatString(L"OscHeardAddRuleFormat",
                            std::wstring{ rule.Address.begin(), rule.Address.end() }, patchbay::OscRuleSummary(rule))));
                        add.Padding(xaml::ThicknessHelper::FromLengths(0, 2, 0, 2));

                        add.Click([weak, endpointId, rule](auto&&, auto&&)
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->ChangeOscRows(endpointId, [rule](patchbay::PatchDocument&, patchbay::PatchEndpoint& target)
                                        {
                                            // The first rules row with room, or a new one on a free group.
                                            for (auto& row : target.Osc->Rows)
                                            {
                                                if (row.Format == patchbay::OscFormat::Custom && row.Rules.size() < patchbay::MaximumOscRules)
                                                {
                                                    row.Rules.push_back(rule);
                                                    return;
                                                }
                                            }

                                            auto const group = patchbay::FreeOscGroup(*target.Osc);

                                            if (!group.has_value())
                                            {
                                                return;
                                            }

                                            patchbay::OscRow row{};
                                            row.Group = *group;
                                            row.Root.clear();
                                            row.Format = patchbay::OscFormat::Custom;
                                            row.Rules.push_back(rule);
                                            target.Osc->Rows.push_back(std::move(row));
                                        });
                                }
                            });

                        entry.Children().Append(add);
                    }
                }

                m_oscHeardPanel.Children().Append(entry);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the OSC connection's status.")
    }

    void MainWindow::RecheckOscFirewall() noexcept
    {
        try
        {
            if (m_oscFirewallChecking)
            {
                return;
            }

            m_oscFirewallChecking = true;

            [](winrt::weak_ref<MainWindow> weak) -> winrt::fire_and_forget
                {
                    co_await patchbay::RunOnBackgroundAsync([]()
                        {
                            patchbay::OscHub::Current().RecheckFirewall();
                        });

                    if (auto strong = weak.get())
                    {
                        strong->m_oscFirewallChecking = false;
                        strong->m_oscFirewallCheckedAt = ::WindowsMidiServicesInternal::GetCurrentMidiTimestamp();
                        strong->UpdateOscFirewallBar();
                    }
                }(get_weak());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read Windows Firewall.")
    }

    void MainWindow::UpdateOscFirewallBar() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            auto const* patch = CurrentPatch();
            auto& library = patchbay::PatchLibrary::Current();

            auto const relevant = patch != nullptr && library.IsRouting(m_patchKey) && patchbay::WantsOscOnEveryNetwork(*patch);

            // Read again now and then, so a rule added in Windows Firewall itself is noticed too.
            if (relevant && !m_oscFirewallAllowing)
            {
                auto const now = ::WindowsMidiServicesInternal::GetCurrentMidiTimestamp();
                auto const frequency = ::WindowsMidiServicesInternal::GetMidiTimestampFrequency();

                if (m_oscFirewallCheckedAt == 0 || now - m_oscFirewallCheckedAt > frequency * FirewallRecheckSeconds)
                {
                    m_oscFirewallCheckedAt = now;
                    RecheckOscFirewall();
                }
            }

            auto const show = relevant && patchbay::OscFirewallHolds(*patch);

            if (!show)
            {
                if (m_oscFirewallBar != nullptr && !m_oscFirewallAllowing)
                {
                    m_oscFirewallBar.IsOpen(false);
                    m_oscFirewallBar.Visibility(xaml::Visibility::Collapsed);
                    m_oscFirewallSignature.clear();
                }

                return;
            }

            auto const state = patchbay::OscHub::Current().Firewall();

            std::wstring signature = std::to_wstring(static_cast<int32_t>(state.Status));

            for (auto const& network : state.Networks)
            {
                signature += std::format(L"|{}:{}:{}", network.Name, network.Type, static_cast<int32_t>(network.Status));
            }

            for (auto const& rule : state.BlockingRuleNames)
            {
                signature += L"|r" + rule;
            }

            if (signature == m_oscFirewallSignature && m_oscFirewallBar != nullptr)
            {
                return;
            }

            m_oscFirewallSignature = signature;

            if (m_oscFirewallBar == nullptr)
            {
                m_oscFirewallBar = controls::InfoBar{};
                m_oscFirewallBar.Title(resources::GetString(L"OscFirewallTitle"));
                m_oscFirewallBar.Severity(controls::InfoBarSeverity::Warning);
                m_oscFirewallBar.IsClosable(true);

                m_oscFirewallBar.Closed([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_oscFirewallBar.Visibility(xaml::Visibility::Collapsed);
                        }
                    });

                MessageBars().Children().InsertAt(0, m_oscFirewallBar);
            }

            // What's blocked, by the names Windows gives the networks.
            std::vector<std::wstring> blocked{};
            long blockedTypes{ 0 };

            for (auto const& network : state.Networks)
            {
                if (!IsBlocked(network.Status))
                {
                    continue;
                }

                std::wstring const name{ network.Name.empty() ? std::wstring{ NetworkTypeText(network.Type) } : network.Name };

                if (std::find(blocked.begin(), blocked.end(), name) == blocked.end())
                {
                    blocked.push_back(name);
                }

                if (network.Status == firewall::FirewallStatus::Closed)
                {
                    blockedTypes |= network.Type;
                }
            }

            auto message = resources::FormatString(L"OscFirewallClosedFormat", JoinNames(blocked));

            if (state.Status == firewall::FirewallStatus::ClosedByRule)
            {
                message = resources::FormatString(L"OscFirewallRuleFormat", message, JoinNames(state.BlockingRuleNames));
            }
            else if (state.Status == firewall::FirewallStatus::ClosedForAll)
            {
                message = resources::FormatString(L"OscFirewallBlockAllFormat", message);
            }

            if (state.ManagedByPolicy)
            {
                message = resources::FormatString(L"OscFirewallManagedFormat", message);
            }

            m_oscFirewallBar.Message(message);

            // One row: what to let it in on, then the button. The longer explanations are in each
            // box's tooltip and help text, so the bar stays short.
            controls::StackPanel choices{};
            choices.Orientation(controls::Orientation::Horizontal);
            choices.Spacing(16);
            choices.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 8));

            controls::CheckBox domainBox{ nullptr };
            controls::CheckBox privateBox{ nullptr };
            controls::CheckBox publicBox{ nullptr };

            auto weak = get_weak();

            auto const explain = [](xaml::FrameworkElement const& element, winrt::hstring const& text)
                {
                    controls::ToolTipService::SetToolTip(element, winrt::box_value(text));
                    xaml::Automation::AutomationProperties::SetHelpText(element, text);
                };

            if (state.Status == firewall::FirewallStatus::Closed)
            {
                auto label = Line(resources::GetString(L"OscFirewallChoose"), 14);
                label.VerticalAlignment(xaml::VerticalAlignment::Center);
                choices.Children().Append(label);

                auto const showDomain = state.JoinedToDomain || (state.ConnectedNetworks & firewall::DomainNetworks) != 0;

                auto const choice = [&](long type, wchar_t const* shortKey, wchar_t const* longKey)
                    {
                        auto text = resources::GetString(shortKey);

                        if ((state.ConnectedNetworks & type) != 0)
                        {
                            text = resources::FormatString(L"OscFirewallConnectedNowFormat", text);
                        }

                        // What's blocked now starts checked, except public networks, which the
                        // customer has to choose.
                        auto box = parts::Check(text, (blockedTypes & type) != 0 && type != firewall::PublicNetworks);
                        box.VerticalAlignment(xaml::VerticalAlignment::Center);

                        auto tip = resources::GetString(longKey);

                        if (type == firewall::PublicNetworks)
                        {
                            tip = tip + L"\n" + resources::GetString(L"OscFirewallPublicHint");
                        }

                        explain(box, tip);
                        choices.Children().Append(box);

                        return box;
                    };

                if (showDomain)
                {
                    domainBox = choice(firewall::DomainNetworks, L"OscFirewallDomainShort", L"OscFirewallDomainNetworks");
                }

                privateBox = choice(firewall::PrivateNetworks, L"OscFirewallPrivateShort", L"OscFirewallPrivateNetworks");
                publicBox = choice(firewall::PublicNetworks, L"OscFirewallPublicShort", L"OscFirewallPublicNetworks");

                controls::Button allow{};
                allow.Content(winrt::box_value(resources::GetString(L"OscFirewallAllowButton")));
                allow.VerticalAlignment(xaml::VerticalAlignment::Center);
                explain(allow, resources::GetString(L"OscFirewallAllowNote"));

                allow.Click([weak, domainBox, privateBox, publicBox, showDomain](auto&&, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        // Without a domain box, the private box covers domain networks too.
                        long const networks =
                            (showDomain && Checked(domainBox) ? firewall::DomainNetworks : 0) |
                            (Checked(privateBox) ? (showDomain ? firewall::PrivateNetworks : firewall::DomainNetworks | firewall::PrivateNetworks) : 0) |
                            (Checked(publicBox) ? firewall::PublicNetworks : 0);

                        if (networks == 0)
                        {
                            strong->m_oscFirewallBar.Message(resources::GetString(L"OscFirewallChooseOne"));
                            return;
                        }

                        strong->AllowOscThroughFirewallAsync(networks);
                    });

                choices.Children().Append(allow);
            }
            else
            {
                // A rule that blocks, or "block all incoming connections": both are the customer's
                // to change in Windows Security. Nothing here removes someone's rule for them.
                controls::Button open{};
                open.Content(winrt::box_value(resources::GetString(L"OscFirewallOpenSettings")));

                open.Click([](auto&&, auto&&)
                    {
                        winrt::Windows::System::Launcher::LaunchUriAsync(foundation::Uri{ L"windowsdefender://network/" });
                    });

                choices.Children().Append(open);
            }

            m_oscFirewallBar.ActionButton(nullptr);
            m_oscFirewallBar.Content(choices);
            m_oscFirewallBar.Visibility(xaml::Visibility::Visible);
            m_oscFirewallBar.IsOpen(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the firewall bar.")
    }

    winrt::fire_and_forget MainWindow::AllowOscThroughFirewallAsync(long networks)
    {
        auto strong = get_strong();

        try
        {
            if (m_oscFirewallAllowing || m_oscFirewallBar == nullptr)
            {
                co_return;
            }

            m_oscFirewallAllowing = true;
            auto const reset = wil::scope_exit([this]() { m_oscFirewallAllowing = false; });

            m_oscFirewallBar.Message(resources::GetString(L"OscFirewallWaiting"));

            if (auto const panel = m_oscFirewallBar.Content().try_as<controls::Panel>())
            {
                for (auto const& child : panel.Children())
                {
                    if (auto const control = child.try_as<controls::Control>())
                    {
                        control.IsEnabled(false);
                    }
                }
            }

            auto const owner = m_chrome.WindowHandle();
            HRESULT result{ E_FAIL };

            co_await patchbay::RunOnBackgroundAsync([&result, owner, networks]()
                {
                    result = firewall::RequestAllow(owner, patchbay::OscHub::FirewallTarget(), networks);

                    // Read again whatever happened, and open every network if it's allowed now.
                    patchbay::OscHub::Current().RecheckFirewall();
                });

            if (m_closing)
            {
                co_return;
            }

            m_oscFirewallSignature.clear();

            if (SUCCEEDED(result))
            {
                ShowStatus(resources::GetString(L"OscFirewallAllowed"), controls::InfoBarSeverity::Success);
            }
            else if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            {
                ShowStatus(resources::GetString(L"OscFirewallCanceled"), controls::InfoBarSeverity::Informational);
            }
            else
            {
                ShowStatus(resources::FormatString(L"OscFirewallFailedFormat", static_cast<uint32_t>(result)), controls::InfoBarSeverity::Error);
            }

            m_oscFirewallAllowing = false;
            UpdateOscFirewallBar();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to allow Windows MIDI Patchbay through Windows Firewall.")
    }
}
