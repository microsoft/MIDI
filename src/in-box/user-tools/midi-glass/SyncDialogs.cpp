// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "SyncDialogs.h"

#include "AppSettings.h"
#include "DeviceLinkDiscovery.h"
#include "DeviceLinkPairing.h"
#include "ProvenanceForm.h"
#include "StringResources.h"
#include "SyncMemory.h"
#include "SyncPack.h"
#include "SyncProtocol.h"

#include <wil/cppwinrt_helpers.h>

#include <shlobj.h>

#include <filesystem>

namespace resources = ::midiglass::resources;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace midiglass::devicesync
{
    namespace
    {
        using winrt::box_value;
        using ::midiglass::sharing::DialogWidth;
        using ::midiglass::sharing::FormatDate;
        using ::midiglass::sharing::Paragraph;
        using glass::sync::PairOutcome;
        using glass::sync::SendOutcome;

        // Per user, never exportable. Named for the app, so a later tool gets a key of its own.
        constexpr wchar_t IdentityKeyName[] = L"Microsoft Windows MIDI Glass device link";

        // How long "Sync to <name>" looks for the last PC before asking which one.
        constexpr int32_t DirectLookMilliseconds = 3000;

        std::mutex g_identityLock{};
        std::shared_ptr<midiapp::DeviceIdentity> g_identity{};

        // Started the first time a layout is sent, and left running: it's the Windows DNS client
        // that looks, and nothing here listens for it.
        midiapp::DeviceLinkBrowser& Browser()
        {
            static auto* browser = []()
                {
                    auto* created = new midiapp::DeviceLinkBrowser{};
                    created->Start(glass::sync::SyncServiceType);
                    return created;
                }();

            return *browser;
        }

        foundation::IAsyncAction ShowNoticeAsync(xaml::XamlRoot root, winrt::hstring title, winrt::hstring body)
        {
            try
            {
                auto text = Paragraph(body);
                text.MaxWidth(DialogWidth);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(root);
                dialog.Title(box_value(title));
                dialog.Content(text);
                dialog.CloseButtonText(resources::GetString(L"DialogClose"));

                co_await dialog.ShowAsync();
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to show a notice.")
        }

        // A dialog that shows work going on, with a ring and one line of text.
        controls::StackPanel BusyPanel(_In_ winrt::hstring const& text)
        {
            controls::StackPanel panel{};
            panel.Orientation(controls::Orientation::Horizontal);
            panel.Spacing(12.0);
            panel.Width(DialogWidth);

            controls::ProgressRing ring{};
            ring.IsActive(true);
            ring.Width(20.0);
            ring.Height(20.0);
            automation::AutomationProperties::SetName(ring, text);

            panel.Children().Append(ring);
            panel.Children().Append(Paragraph(text));

            return panel;
        }

        winrt::hstring PackProblemText(_In_ midiapp::ContentPackProblem problem)
        {
            using midiapp::ContentPackProblem;

            switch (problem)
            {
            case ContentPackProblem::TooBig:
            case ContentPackProblem::TooManyEntries:
                return resources::GetString(L"PackProblemTooBig");
            case ContentPackProblem::NewerFormat:
                return resources::GetString(L"PackProblemNewer");
            case ContentPackProblem::BrokenSignature:
                return resources::GetString(L"PackProblemBrokenSignature");
            case ContentPackProblem::RevokedSignature:
                return resources::GetString(L"PackProblemRevoked");
            default:
                return resources::GetString(L"PackProblemDamaged");
            }
        }

        // ---------------------------------------------------------------- choosing a PC

        struct Choice
        {
            midiapp::DeviceLinkFoundDevice Device{};
            bool Paired{ false };
        };

        bool IsPaired(_In_ std::vector<midiapp::PairedDevice> const& paired, _In_ midiapp::DeviceLinkFoundDevice const& device)
        {
            return device.Key.has_value() && std::any_of(paired.begin(), paired.end(),
                [&device](midiapp::PairedDevice const& p) { return midiapp::SameDeviceKey(p.Key, *device.Key); });
        }

        std::vector<Choice> CurrentChoices(_In_ midiapp::DeviceKeyHash const& ownKey)
        {
            auto const paired = midiapp::PairedDeviceList{ PairedListPath() }.Load();

            std::vector<Choice> choices{};

            for (auto& device : Browser().Found(&ownKey))
            {
                Choice choice{};
                choice.Paired = IsPaired(paired, device);
                choice.Device = std::move(device);

                choices.push_back(std::move(choice));
            }

            return choices;
        }

        std::wstring ChoiceSignature(_In_ std::vector<Choice> const& choices)
        {
            std::wstring signature{};

            for (auto const& choice : choices)
            {
                signature += choice.Device.Name + L"|" + choice.Device.Host + L"|" + std::to_wstring(choice.Device.Port) + L"|" +
                    (choice.Device.Key ? midiapp::DeviceKeyHashToHex(*choice.Device.Key) : std::wstring{}) +
                    (choice.Paired ? L"|p\n" : L"|\n");
            }

            return signature;
        }

        controls::ListViewItem ChoiceRow(_In_ Choice const& choice)
        {
            controls::StackPanel text{};
            text.Spacing(2.0);
            text.Padding({ 0, 6, 0, 6 });

            controls::TextBlock name{};
            name.Text(choice.Device.Name);
            name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

            auto const detail = choice.Paired
                ? resources::FormatString(L"SyncPickPairedFormat", midiapp::ShortDeviceFingerprint(*choice.Device.Key))
                : resources::GetString(L"SyncPickNotPaired");

            text.Children().Append(name);
            text.Children().Append(Paragraph(detail, 12.0, L"TextFillColorSecondaryBrush"));

            controls::ListViewItem item{};
            item.Content(text);
            automation::AutomationProperties::SetName(item, winrt::hstring{ choice.Device.Name + L", " + std::wstring{ detail } });

            return item;
        }

        // The PC to send to, or nothing when the person cancels.
        foundation::IAsyncOperation<foundation::IInspectable> PickAsync(
            xaml::XamlRoot root,
            midiapp::DeviceKeyHash ownKey,
            std::wstring layoutName,
            std::shared_ptr<std::optional<Choice>> picked)
        {
            auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            auto choices = std::make_shared<std::vector<Choice>>();
            auto shown = std::make_shared<std::wstring>();

            controls::ListView list{};
            list.SelectionMode(controls::ListViewSelectionMode::Single);
            list.MinHeight(120.0);
            list.MaxHeight(320.0);
            automation::AutomationProperties::SetName(list, resources::GetString(L"SyncPickListName"));

            auto empty = Paragraph(resources::GetString(L"SyncPickLooking"), 13.0, L"TextFillColorSecondaryBrush");
            empty.MaxWidth(DialogWidth);

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Width(DialogWidth);
            panel.Children().Append(Paragraph(resources::FormatString(L"SyncPickIntroFormat", layoutName), 13.0));
            panel.Children().Append(list);
            panel.Children().Append(empty);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::GetString(L"SyncPickTitle")));
            dialog.Content(panel);
            dialog.PrimaryButtonText(resources::GetString(L"SyncPickSend"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.IsPrimaryButtonEnabled(false);

            auto const lastKey = midiapp::DeviceKeyHashFromHex(AppSettings::Current().SyncDeviceKey());

            auto const refresh = [list, empty, dialog, choices, shown, ownKey, lastKey]()
                {
                    try
                    {
                        auto latest = CurrentChoices(ownKey);
                        auto signature = ChoiceSignature(latest);

                        if (signature == *shown)
                        {
                            return;
                        }

                        std::optional<midiapp::DeviceKeyHash> selectedKey{};
                        auto const selected = list.SelectedIndex();

                        if (selected >= 0 && static_cast<size_t>(selected) < choices->size())
                        {
                            selectedKey = (*choices)[static_cast<size_t>(selected)].Device.Key;
                        }
                        else
                        {
                            selectedKey = lastKey;
                        }

                        *choices = std::move(latest);
                        *shown = std::move(signature);

                        list.Items().Clear();

                        int32_t select{ -1 };

                        for (size_t i = 0; i < choices->size(); ++i)
                        {
                            list.Items().Append(ChoiceRow((*choices)[i]));

                            auto const& key = (*choices)[i].Device.Key;

                            if (select < 0 && selectedKey && key && midiapp::SameDeviceKey(*key, *selectedKey))
                            {
                                select = static_cast<int32_t>(i);
                            }
                        }

                        list.SelectedIndex(select);
                        empty.Visibility(choices->empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                        dialog.IsPrimaryButtonEnabled(list.SelectedIndex() >= 0);
                    }
                    MIDI_GLASS_CATCH_AND_LOG(L"Unable to list the PCs to send to.")
                };

            list.SelectionChanged([dialog, list](auto&&, auto&&)
                {
                    dialog.IsPrimaryButtonEnabled(list.SelectedIndex() >= 0);
                });

            auto timer = dispatcher.CreateTimer();
            timer.Interval(std::chrono::seconds{ 1 });
            timer.Tick([refresh](auto&&, auto&&) { refresh(); });

            refresh();
            timer.Start();

            auto const answer = co_await dialog.ShowAsync();

            timer.Stop();

            auto const index = list.SelectedIndex();

            if (answer == controls::ContentDialogResult::Primary && index >= 0 && static_cast<size_t>(index) < choices->size())
            {
                *picked = (*choices)[static_cast<size_t>(index)];
            }

            co_return nullptr;
        }

        // ---------------------------------------------------------------- pairing, this side

        // Pairs with the chosen PC: connects, shows the code, and remembers it only when the
        // people at both PCs say the codes match. The other PC's key, or nothing.
        foundation::IAsyncOperation<foundation::IInspectable> PairAsync(
            xaml::XamlRoot root,
            std::shared_ptr<midiapp::DeviceIdentity> identity,
            midiapp::DeviceLinkFoundDevice device,
            std::shared_ptr<std::optional<midiapp::DeviceKeyHash>> pairedKey)
        {
            auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            auto initiator = std::make_shared<glass::sync::SyncPairingInitiator>();

            // 0 while connecting, 1 once the code is up, 2 once this code closed it.
            auto stage = std::make_shared<std::atomic<int32_t>>(0);
            auto canceled = std::make_shared<std::atomic<bool>>(false);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::FormatString(L"SyncPairTitleFormat", device.Name)));
            dialog.Content(BusyPanel(resources::FormatString(L"SyncPairConnectingFormat", device.Name)));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));

            dialog.Closed([initiator, stage, canceled](auto&&, auto&&)
                {
                    if (stage->load() == 0)
                    {
                        canceled->store(true);
                        initiator->Cancel();
                    }
                });

            auto showing = dialog.ShowAsync();

            co_await winrt::resume_background();

            auto const started = initiator->Start(device.Host, device.Port, *identity, midiapp::ThisDeviceLinkName());

            co_await wil::resume_foreground(dispatcher);

            if (canceled->load())
            {
                co_await showing;
                co_return nullptr;
            }

            if (started != PairOutcome::CodeReady)
            {
                stage->store(2);
                dialog.Hide();
                co_await showing;

                co_await ShowNoticeAsync(root, resources::GetString(L"SyncPairFailedTitle"),
                    started == PairOutcome::NotPairing
                        ? resources::FormatString(L"SyncPairNotOpenFormat", device.Name)
                        : started == PairOutcome::CouldNotConnect
                            ? resources::FormatString(L"SyncCouldNotConnectFormat", device.Name)
                            : resources::GetString(L"SyncPairFailedBody"));

                co_return nullptr;
            }

            stage->store(1);

            controls::TextBlock code{};
            code.Text(initiator->Code());
            code.FontSize(40.0);
            code.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            code.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            code.IsTextSelectionEnabled(false);
            automation::AutomationProperties::SetName(code, resources::FormatString(L"SyncPairCodeNameFormat", initiator->Code()));

            controls::StackPanel panel{};
            panel.Spacing(14.0);
            panel.Width(DialogWidth);
            panel.Children().Append(Paragraph(resources::FormatString(L"SyncPairCompareFormat", initiator->PeerName()), 14.0));
            panel.Children().Append(code);
            panel.Children().Append(Paragraph(resources::GetString(L"SyncPairWhy"), 12.0, L"TextFillColorSecondaryBrush"));

            dialog.Content(panel);
            dialog.PrimaryButtonText(resources::GetString(L"SyncPairMatch"));
            dialog.CloseButtonText(resources::GetString(L"SyncPairNoMatch"));
            dialog.DefaultButton(controls::ContentDialogButton::None);

            auto const answer = co_await showing;
            auto const matched = answer == controls::ContentDialogResult::Primary;

            // The other person may still be deciding.
            controls::ContentDialog waiting{};
            waiting.XamlRoot(root);
            waiting.Title(box_value(resources::FormatString(L"SyncPairTitleFormat", initiator->PeerName())));
            waiting.Content(BusyPanel(resources::FormatString(L"SyncPairWaitingFormat", initiator->PeerName())));
            waiting.CloseButtonText(resources::GetString(L"DialogCancel"));

            auto waitingDone = std::make_shared<std::atomic<bool>>(false);

            waiting.Closed([initiator, waitingDone](auto&&, auto&&)
                {
                    if (!waitingDone->load())
                    {
                        initiator->Cancel();
                    }
                });

            foundation::IAsyncOperation<controls::ContentDialogResult> waitingShown{ nullptr };

            if (matched)
            {
                waitingShown = waiting.ShowAsync();
            }

            co_await winrt::resume_background();

            midiapp::PairedDeviceList paired{ PairedListPath() };
            auto const finished = initiator->Finish(matched, paired);

            co_await wil::resume_foreground(dispatcher);

            waitingDone->store(true);

            if (waitingShown != nullptr)
            {
                waiting.Hide();
                co_await waitingShown;
            }

            if (finished == PairOutcome::Paired)
            {
                *pairedKey = initiator->PeerKey();
                co_return nullptr;
            }

            if (finished != PairOutcome::Canceled)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"SyncPairFailedTitle"),
                    resources::GetString(finished == PairOutcome::Declined ? L"SyncPairDeclinedBody" : L"SyncPairFailedBody"));
            }

            co_return nullptr;
        }

        // ---------------------------------------------------------------- sending

        winrt::hstring DescribeSend(
            _In_ glass::sync::SendResult const& result,
            _In_ std::wstring const& deviceName,
            _In_ std::wstring const& layoutName)
        {
            switch (result.Outcome)
            {
            case SendOutcome::Sent:
                return result.FilesSkipped > 0
                    ? resources::FormatString(L"SyncSentDeltaFormat", layoutName, deviceName,
                        std::to_wstring(result.FilesSent), std::to_wstring(result.FilesSkipped))
                    : resources::FormatString(L"SyncSentFormat", layoutName, deviceName, std::to_wstring(result.FilesSent));
            case SendOutcome::CouldNotConnect:
                return resources::FormatString(L"SyncCouldNotConnectFormat", deviceName);
            case SendOutcome::NotThePairedDevice:
                return resources::FormatString(L"SyncWrongKeyFormat", deviceName);
            case SendOutcome::NotPaired:
                return resources::FormatString(L"SyncNotPairedFormat", deviceName);
            case SendOutcome::Refused:
                return resources::FormatString(L"SyncRefusedFormat", deviceName);
            case SendOutcome::PackProblem:
                return resources::FormatString(L"SyncReceiverProblemFormat", deviceName, std::wstring{ PackProblemText(result.ReceiverProblem) });
            default:
                return resources::FormatString(L"SyncFailedFormat", deviceName);
            }
        }
    }

    // -------------------------------------------------------------------- shared

    midiapp::firewall::FirewallTarget FirewallTarget()
    {
        midiapp::firewall::FirewallTarget target{};

        target.ProgramPath = midiapp::firewall::CurrentProgramPath();
        target.ProgramFileName = L"\\" + std::filesystem::path{ target.ProgramPath }.filename().wstring();
        target.RuleName = std::wstring{ resources::GetString(L"SyncFirewallRuleName") };
        target.RuleDescription = std::wstring{ resources::GetString(L"SyncFirewallRuleDescription") };
        target.AllowSwitch = FirewallAllowSwitch;
        target.Protocols = { midiapp::firewall::TcpProtocol };
        target.RemoteAddresses = L"LocalSubnet";

        return target;
    }

    _Use_decl_annotations_
    std::wstring DataFile(std::wstring const& name)
    {
        PWSTR raw{ nullptr };

        if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)))
        {
            return {};
        }

        std::filesystem::path folder{ raw };
        ::CoTaskMemFree(raw);

        folder /= L"Microsoft";
        folder /= L"MIDI Glass";

        std::error_code ignored{};
        std::filesystem::create_directories(folder, ignored);

        return (folder / name).wstring();
    }

    std::wstring PairedListPath()
    {
        return DataFile(L"PairedDevices.json");
    }

    std::shared_ptr<midiapp::DeviceIdentity> Identity() noexcept
    {
        try
        {
            std::scoped_lock guard{ g_identityLock };

            if (g_identity == nullptr)
            {
                HRESULT error{ E_FAIL };
                g_identity = std::shared_ptr<midiapp::DeviceIdentity>{ midiapp::DeviceIdentity::OpenOrCreate(IdentityKeyName, error) };

                if (g_identity == nullptr)
                {
                    LOG_HR_MSG(error, "Unable to open the device key for Sync to device.");
                }
            }

            return g_identity;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void Unpair(midiapp::DeviceKeyHash const& key) noexcept
    {
        try
        {
            auto const hex = midiapp::DeviceKeyHashToHex(key);

            midiapp::PairedDeviceList{ PairedListPath() }.Forget(key);
            glass::sync::SessionApprovals::Current().ForgetSender(hex);
            glass::sync::SyncRecords{ DataFile(L"SyncRecords.json") }.ForgetSender(hex);

            if (AppSettings::Current().SyncDeviceKey() == hex)
            {
                AppSettings::Current().SyncDeviceKey({});
            }
        }
        catch (...)
        {
        }
    }

    std::wstring LastDeviceName() noexcept
    {
        try
        {
            auto const key = midiapp::DeviceKeyHashFromHex(AppSettings::Current().SyncDeviceKey());

            if (!key)
            {
                return {};
            }

            auto const found = midiapp::PairedDeviceList{ PairedListPath() }.Find(*key);

            return found ? found->Name : std::wstring{};
        }
        catch (...)
        {
            return {};
        }
    }

    // -------------------------------------------------------------------- sending a layout

    _Use_decl_annotations_
    foundation::IAsyncAction SendLayoutAsync(xaml::XamlRoot root, std::wstring layoutFilePath, bool direct)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            co_await winrt::resume_background();

            auto const identity = Identity();

            glass::sync::SyncSource source{};
            std::wstring failureKey{};
            auto const prepared = identity != nullptr && glass::sync::PrepareSyncSource(layoutFilePath, source, failureKey);

            // Starts looking now, so the list has something in it by the time it shows.
            Browser();

            std::optional<Choice> chosen{};

            if (prepared && direct)
            {
                if (auto const lastKey = midiapp::DeviceKeyHashFromHex(AppSettings::Current().SyncDeviceKey());
                    lastKey && midiapp::PairedDeviceList{ PairedListPath() }.Find(*lastKey))
                {
                    for (int32_t waited = 0; waited <= DirectLookMilliseconds && !chosen; waited += 250)
                    {
                        for (auto const& choice : CurrentChoices(identity->KeyHash()))
                        {
                            if (choice.Paired && midiapp::SameDeviceKey(*choice.Device.Key, *lastKey))
                            {
                                chosen = choice;
                                break;
                            }
                        }

                        if (!chosen)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
                        }
                    }
                }
            }

            co_await wil::resume_foreground(dispatcher);

            if (identity == nullptr)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"SyncFailedTitle"), resources::GetString(L"SyncNoKeyBody"));
                co_return;
            }

            if (!prepared)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"SyncFailedTitle"), resources::GetString(failureKey.c_str()));
                co_return;
            }

            auto const layoutName = source.Offer.LayoutName.empty()
                ? std::filesystem::path{ layoutFilePath }.stem().wstring()
                : source.Offer.LayoutName;

            if (!chosen)
            {
                auto picked = std::make_shared<std::optional<Choice>>();
                co_await PickAsync(root, identity->KeyHash(), layoutName, picked);

                if (!picked->has_value())
                {
                    co_return;
                }

                chosen = **picked;
            }

            auto const device = chosen->Device;
            midiapp::DeviceKeyHash pairedKey{};

            if (chosen->Paired)
            {
                pairedKey = *device.Key;
            }
            else
            {
                auto paired = std::make_shared<std::optional<midiapp::DeviceKeyHash>>();
                co_await PairAsync(root, identity, device, paired);

                if (!paired->has_value())
                {
                    co_return;
                }

                pairedKey = **paired;
            }

            AppSettings::Current().SyncDeviceKey(midiapp::DeviceKeyHashToHex(pairedKey));

            // Sending, with a bar and a way out.
            auto sender = std::make_shared<glass::sync::SyncSender>();
            auto done = std::make_shared<std::atomic<bool>>(false);
            auto lastPercent = std::make_shared<std::atomic<int32_t>>(-1);

            controls::ProgressBar bar{};
            bar.Minimum(0.0);
            bar.Maximum(100.0);
            bar.IsIndeterminate(true);
            automation::AutomationProperties::SetName(bar, resources::FormatString(L"SyncSendingFormat", layoutName, device.Name));

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Width(DialogWidth);
            panel.Children().Append(Paragraph(resources::FormatString(L"SyncSendingFormat", layoutName, device.Name)));
            panel.Children().Append(bar);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::GetString(L"SyncSendingTitle")));
            dialog.Content(panel);
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));

            dialog.Closed([sender, done](auto&&, auto&&)
                {
                    if (!done->load())
                    {
                        sender->Cancel();
                    }
                });

            auto showing = dialog.ShowAsync();

            co_await winrt::resume_background();

            auto const result = sender->Send(device.Host, device.Port, *identity, midiapp::ThisDeviceLinkName(), pairedKey, source,
                // Runs on the sending thread, so it holds the bar only weakly and touches it on the UI thread.
                [dispatcher, weakBar = winrt::make_weak(bar), lastPercent](uint64_t sent, uint64_t total)
                {
                    auto const percent = total == 0 ? 100 : static_cast<int32_t>(sent * 100 / total);

                    if (lastPercent->exchange(percent) == percent)
                    {
                        return;
                    }

                    dispatcher.TryEnqueue([weakBar, percent]()
                        {
                            if (auto const bar = weakBar.get())
                            {
                                bar.IsIndeterminate(false);
                                bar.Value(percent);
                            }
                        });
                });

            co_await wil::resume_foreground(dispatcher);

            done->store(true);
            dialog.Hide();
            co_await showing;

            if (result.Outcome == SendOutcome::Canceled)
            {
                co_return;
            }

            co_await ShowNoticeAsync(
                root,
                resources::GetString(result.Outcome == SendOutcome::Sent ? L"SyncSentTitle" : L"SyncFailedTitle"),
                DescribeSend(result, device.Name, layoutName));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to send the layout to another PC.")
    }

    // -------------------------------------------------------------------- paired PCs

    _Use_decl_annotations_
    foundation::IAsyncAction ShowPairedDevicesAsync(xaml::XamlRoot root)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            co_await winrt::resume_background();

            auto const identity = Identity();
            auto const ownName = midiapp::ThisDeviceLinkName();

            co_await wil::resume_foreground(dispatcher);

            controls::StackPanel rows{};
            rows.Spacing(4.0);

            auto empty = Paragraph(resources::GetString(L"PairedNone"), 13.0, L"TextFillColorSecondaryBrush");

            // Rebuilt after every removal, from the file.
            auto fill = std::make_shared<std::function<void()>>();

            *fill = [rows, empty, weakFill = std::weak_ptr<std::function<void()>>(fill)]()
                {
                    try
                    {
                        rows.Children().Clear();

                        auto const devices = midiapp::PairedDeviceList{ PairedListPath() }.Load();

                        empty.Visibility(devices.empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                        for (auto const& device : devices)
                        {
                            controls::Grid row{};
                            row.ColumnSpacing(12.0);
                            row.Padding({ 0, 6, 0, 6 });

                            controls::ColumnDefinition textColumn{};
                            textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));
                            controls::ColumnDefinition buttonColumn{};
                            buttonColumn.Width(xaml::GridLengthHelper::Auto());

                            row.ColumnDefinitions().Append(textColumn);
                            row.ColumnDefinitions().Append(buttonColumn);

                            controls::StackPanel text{};
                            text.Spacing(2.0);

                            controls::TextBlock name{};
                            name.Text(device.Name.empty() ? std::wstring{ resources::GetString(L"PairedUnnamed") } : device.Name);
                            name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                            name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

                            auto const detail = device.LastUsed > 0
                                ? resources::FormatString(L"PairedDetailFormat", midiapp::ShortDeviceFingerprint(device.Key), FormatDate(device.LastUsed))
                                : resources::FormatString(L"PairedKeyFormat", midiapp::ShortDeviceFingerprint(device.Key));

                            text.Children().Append(name);
                            text.Children().Append(Paragraph(detail, 12.0, L"TextFillColorSecondaryBrush"));

                            controls::Button remove{};
                            remove.Content(box_value(resources::GetString(L"PairedRemove")));
                            remove.VerticalAlignment(xaml::VerticalAlignment::Center);
                            automation::AutomationProperties::SetName(remove,
                                resources::FormatString(L"PairedRemoveNameFormat", name.Text()));

                            auto const key = device.Key;

                            remove.Click([key, weakFill](auto&&, auto&&)
                                {
                                    Unpair(key);

                                    if (auto const again = weakFill.lock())
                                    {
                                        (*again)();
                                    }
                                });

                            controls::Grid::SetColumn(text, 0);
                            controls::Grid::SetColumn(remove, 1);
                            row.Children().Append(text);
                            row.Children().Append(remove);

                            rows.Children().Append(row);
                        }
                    }
                    MIDI_GLASS_CATCH_AND_LOG(L"Unable to list the paired PCs.")
                };

            (*fill)();

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Width(DialogWidth);
            panel.Children().Append(Paragraph(resources::GetString(L"PairedIntro"), 13.0));

            if (identity != nullptr)
            {
                panel.Children().Append(Paragraph(
                    resources::FormatString(L"SyncThisPcFormat", ownName, midiapp::ShortDeviceFingerprint(identity->KeyHash())),
                    12.0, L"TextFillColorSecondaryBrush"));
            }

            panel.Children().Append(rows);
            panel.Children().Append(empty);

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.MaxHeight(520.0);
            scroller.Padding({ 0, 0, 16, 0 });

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::GetString(L"PairedTitle")));
            dialog.Content(scroller);
            dialog.CloseButtonText(resources::GetString(L"DialogClose"));

            co_await dialog.ShowAsync();

            // Holds the rows' handlers until the dialog is gone.
            fill.reset();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the paired PCs.")
    }
}
