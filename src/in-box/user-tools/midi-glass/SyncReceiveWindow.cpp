// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Receive screen. While it's open, and only then, this PC listens for paired PCs and says on
// the network that it's ready. Closing it stops both. What arrives goes through the same import
// dialog and install as a pack file (sharing::ImportOpenedPackAsync).

#include "pch.h"

#include "SyncDialogs.h"

#include "App.xaml.h"
#include "DeviceLinkDiscovery.h"
#include "DeviceLinkPairing.h"
#include "LayoutPack.h"
#include "LayoutStore.h"
#include "ProvenanceForm.h"
#include "SharingDialogs.h"
#include "StringResources.h"
#include "SyncFileIndex.h"
#include "SyncMemory.h"
#include "SyncProtocol.h"
#include "ThemeStore.h"

#include <wil/cppwinrt_helpers.h>

#include <deque>
#include <filesystem>

namespace resources = ::midiglass::resources;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace midiglass::devicesync
{
    namespace
    {
        using winrt::box_value;
        using ::midiglass::sharing::DialogWidth;
        using ::midiglass::sharing::Heading;
        using ::midiglass::sharing::Paragraph;

        namespace firewall = ::midiapp::firewall;

        constexpr size_t MaximumLogLines = 50;

        bool LetsIn(_In_ firewall::FirewallStatus status) noexcept
        {
            return status == firewall::FirewallStatus::Open || status == firewall::FirewallStatus::Off;
        }

        class ReceiveScreen;

        // What the receiver's own thread may touch. It never holds the screen itself: the last
        // reference to a XAML object has to go on the UI thread, so work for the screen is posted
        // there and the screen is looked up only once it runs.
        struct ReceiveBridge
        {
            winrt::Microsoft::UI::Dispatching::DispatcherQueue Dispatcher{ nullptr };
            std::weak_ptr<ReceiveScreen> Screen{};
            std::atomic<bool> PairingOpen{ false };

            void Post(_In_ std::function<void(ReceiveScreen&)> work);
        };

        class ReceiveScreen : public std::enable_shared_from_this<ReceiveScreen>
        {
        public:
            void Open(_In_ std::function<void(std::wstring const&)> installed);
            void Activate();

        private:
            winrt::fire_and_forget StartAsync();
            winrt::fire_and_forget CheckNetworkAsync();
            winrt::fire_and_forget AllowAsync();
            winrt::fire_and_forget PairAsync();
            winrt::fire_and_forget ProcessQueueAsync();
            foundation::IAsyncAction ImportAsync(glass::sync::ReceivedSync item);

            friend struct ReceiveBridge;

            void Stop() noexcept;
            void Log(_In_ winrt::hstring const& line);
            void ShowCode(_In_ std::shared_ptr<glass::sync::PendingPairing> const& pending);

            HWND WindowHandle() const;

            xaml::Window m_window{ nullptr };
            winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

            std::shared_ptr<glass::sync::SyncReceiver> m_receiver{};
            std::shared_ptr<midiapp::DeviceLinkAdvertiser> m_advertiser{};

            std::shared_ptr<ReceiveBridge> m_bridge{};
            std::shared_ptr<glass::sync::PendingPairing> m_pending{};
            controls::ContentDialog m_pairingDialog{ nullptr };

            std::deque<glass::sync::ReceivedSync> m_queue{};
            bool m_importing{ false };
            bool m_closed{ false };

            // Only touched on the UI thread.
            bool m_checking{ false };
            bool m_starting{ false };
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_recheck{ nullptr };

            controls::TextBlock m_status{ nullptr };
            controls::TextBlock m_thisPc{ nullptr };
            controls::TextBlock m_network{ nullptr };
            controls::Button m_allow{ nullptr };
            controls::HyperlinkButton m_networkSettings{ nullptr };
            controls::Button m_pair{ nullptr };
            controls::StackPanel m_log{ nullptr };

            std::wstring m_ownName{};
            std::function<void(std::wstring const&)> m_installed{};
        };

        std::shared_ptr<ReceiveScreen> g_screen{};

        HWND ReceiveScreen::WindowHandle() const
        {
            HWND handle{ nullptr };

            if (m_window != nullptr)
            {
                m_window.as<IWindowNative>()->get_WindowHandle(&handle);
            }

            return handle;
        }

        _Use_decl_annotations_
        void ReceiveBridge::Post(std::function<void(ReceiveScreen&)> work)
        {
            try
            {
                if (Dispatcher == nullptr)
                {
                    return;
                }

                Dispatcher.TryEnqueue([weak = Screen, work = std::move(work)]()
                    {
                        try
                        {
                            if (auto const screen = weak.lock(); screen != nullptr && !screen->m_closed)
                            {
                                work(*screen);
                            }
                        }
                        MIDI_GLASS_CATCH_AND_LOG(L"Unable to update the Receive screen.")
                    });
            }
            catch (...)
            {
            }
        }

        _Use_decl_annotations_
        void ReceiveScreen::Log(winrt::hstring const& line)
        {
            auto text = Paragraph(line, 13.0);
            m_log.Children().InsertAt(0, text);

            while (m_log.Children().Size() > MaximumLogLines)
            {
                m_log.Children().RemoveAtEnd();
            }

            // m_status is a live region, so a person using a screen reader hears what arrived.
            m_status.Text(line);
        }

        _Use_decl_annotations_
        void ReceiveScreen::Open(std::function<void(std::wstring const&)> installed)
        {
            m_installed = std::move(installed);
            m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            m_ownName = midiapp::ThisDeviceLinkName();

            m_bridge = std::make_shared<ReceiveBridge>();
            m_bridge->Dispatcher = m_dispatcher;
            m_bridge->Screen = weak_from_this();

            m_window = xaml::Window{};
            m_window.Title(resources::GetString(L"ReceiveTitle"));
            m_window.SystemBackdrop(media::MicaBackdrop{});

            controls::StackPanel panel{};
            panel.Spacing(14.0);
            panel.Padding({ 28, 24, 28, 24 });
            panel.MaxWidth(620.0);
            panel.HorizontalAlignment(xaml::HorizontalAlignment::Left);

            auto heading = Heading(resources::GetString(L"ReceiveTitle"));
            heading.FontSize(22.0);
            automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level1);
            panel.Children().Append(heading);

            panel.Children().Append(Paragraph(resources::GetString(L"ReceiveIntro"), 14.0));

            m_thisPc = Paragraph(resources::FormatString(L"ReceiveThisPcFormat", m_ownName), 13.0, L"TextFillColorSecondaryBrush");
            panel.Children().Append(m_thisPc);

            m_network = Paragraph(resources::GetString(L"ReceiveCheckingNetwork"), 13.0);
            panel.Children().Append(m_network);

            controls::StackPanel networkButtons{};
            networkButtons.Orientation(controls::Orientation::Horizontal);
            networkButtons.Spacing(8.0);

            m_allow = controls::Button{};
            m_allow.Content(box_value(resources::GetString(L"ReceiveAllowFirewall")));
            m_allow.Visibility(xaml::Visibility::Collapsed);
            m_allow.Click([weak = weak_from_this()](auto&&, auto&&)
                {
                    if (auto const screen = weak.lock())
                    {
                        screen->AllowAsync();
                    }
                });

            m_networkSettings = controls::HyperlinkButton{};
            m_networkSettings.Content(box_value(resources::GetString(L"ReceiveNetworkSettings")));
            m_networkSettings.NavigateUri(foundation::Uri{ L"ms-settings:network-status" });
            m_networkSettings.Visibility(xaml::Visibility::Collapsed);

            networkButtons.Children().Append(m_allow);
            networkButtons.Children().Append(m_networkSettings);
            panel.Children().Append(networkButtons);

            m_status = Paragraph(resources::GetString(L"ReceiveStarting"), 15.0);
            m_status.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            midiapp::MakeLiveStatusRegion(m_status);
            panel.Children().Append(m_status);

            m_pair = controls::Button{};
            m_pair.Content(box_value(resources::GetString(L"ReceivePair")));
            m_pair.IsEnabled(false);
            m_pair.Click([weak = weak_from_this()](auto&&, auto&&)
                {
                    if (auto const screen = weak.lock())
                    {
                        screen->PairAsync();
                    }
                });
            panel.Children().Append(m_pair);

            auto logHeading = Heading(resources::GetString(L"ReceiveLogHeading"));
            automation::AutomationProperties::SetHeadingLevel(logHeading, automation::Peers::AutomationHeadingLevel::Level2);
            panel.Children().Append(logHeading);

            m_log = controls::StackPanel{};
            m_log.Spacing(6.0);
            panel.Children().Append(m_log);

            controls::ScrollViewer scroller{};
            scroller.Content(panel);

            m_window.Content(scroller);

            try
            {
                auto const dpi = ::GetDpiForWindow(WindowHandle());
                auto const scale = dpi == 0 ? 1.0 : dpi / 96.0;

                m_window.AppWindow().Resize({ static_cast<int32_t>(600 * scale), static_cast<int32_t>(700 * scale) });
            }
            catch (...)
            {
            }

            m_window.Closed([weak = weak_from_this()](auto&&, auto&&)
                {
                    if (auto const screen = weak.lock())
                    {
                        screen->Stop();
                    }

                    g_screen = nullptr;
                });

            m_window.Activate();

            m_recheck = m_dispatcher.CreateTimer();
            m_recheck.Interval(std::chrono::seconds{ 5 });
            m_recheck.Tick([weak = weak_from_this()](auto&&, auto&&)
                {
                    if (auto const screen = weak.lock())
                    {
                        screen->CheckNetworkAsync();
                    }
                });

            CheckNetworkAsync();
        }

        void ReceiveScreen::Activate()
        {
            if (m_window != nullptr)
            {
                m_window.Activate();
            }
        }

        winrt::fire_and_forget ReceiveScreen::StartAsync()
        {
            auto self = shared_from_this();
            auto const bridge = m_bridge;

            m_starting = true;

            try
            {
                co_await winrt::resume_background();

                auto const identity = Identity();

                HRESULT started{ E_FAIL };
                auto receiver = std::make_shared<glass::sync::SyncReceiver>();
                auto advertiser = std::make_shared<midiapp::DeviceLinkAdvertiser>();

                if (identity != nullptr)
                {
                    glass::sync::SyncReceiverOptions options{};
                    options.Identity = identity;
                    options.OwnName = m_ownName;
                    options.PairedListPath = PairedListPath();
                    options.Folders = { glass::LayoutsFolder(), glass::ThemesFolder() };
                    options.FileIndexPath = DataFile(L"SyncFileIndex.json");

                    options.IsPairingOpen = [bridge]()
                        {
                            return bridge->PairingOpen.load();
                        };

                    options.PairingAsked = [bridge](std::shared_ptr<glass::sync::PendingPairing> const& pending)
                        {
                            bridge->Post([pending](ReceiveScreen& s) { s.ShowCode(pending); });
                        };

                    options.PairingFinished = [bridge](std::shared_ptr<glass::sync::PendingPairing> const& pending, bool paired)
                        {
                            auto const name = pending->PeerName;

                            bridge->Post([name, paired](ReceiveScreen& s)
                                {
                                    s.Log(resources::FormatString(paired ? L"ReceivePairedFormat" : L"ReceiveNotPairedFormat", name));
                                });
                        };

                    options.SyncStarted = [bridge](std::wstring const& sender, std::wstring const& layout)
                        {
                            bridge->Post([sender, layout](ReceiveScreen& s)
                                {
                                    s.m_status.Text(resources::FormatString(L"ReceiveReceivingFormat", layout, sender));
                                });
                        };

                    options.SyncFailed = [bridge](std::wstring const& sender, std::wstring const& layout, midiapp::ContentPackProblem)
                        {
                            bridge->Post([sender, layout](ReceiveScreen& s)
                                {
                                    s.Log(resources::FormatString(L"ReceiveFailedFormat", layout, sender));
                                });
                        };

                    options.SyncReceived = [bridge](glass::sync::ReceivedSync&& received)
                        {
                            auto shared = std::make_shared<glass::sync::ReceivedSync>(std::move(received));

                            bridge->Post([shared](ReceiveScreen& s)
                                {
                                    s.m_queue.push_back(std::move(*shared));
                                    s.ProcessQueueAsync();
                                });
                        };
                    started = receiver->Start(std::move(options));

                    if (SUCCEEDED(started))
                    {
                        advertiser->Start(m_ownName + L" (MIDI Glass)", glass::sync::SyncServiceType, receiver->Port(),
                            m_ownName, identity->KeyHash());
                    }
                }

                co_await wil::resume_foreground(m_dispatcher);

                m_starting = false;

                if (m_closed)
                {
                    // Closed while starting: stopped here instead, off the UI thread.
                    std::thread([receiver, advertiser]() { advertiser->Stop(); receiver->Stop(); }).detach();
                    co_return;
                }

                if (identity == nullptr || FAILED(started))
                {
                    m_status.Text(resources::GetString(identity == nullptr ? L"SyncNoKeyBody" : L"ReceiveCouldNotListen"));
                    co_return;
                }

                m_receiver = receiver;
                m_advertiser = advertiser;

                m_thisPc.Text(resources::FormatString(L"SyncThisPcFormat", m_ownName, midiapp::ShortDeviceFingerprint(identity->KeyHash())));
                m_status.Text(resources::GetString(L"ReceiveWaiting"));
                m_pair.IsEnabled(!m_importing);
            }
            catch (...)
            {
                m_starting = false;
                LOG_CAUGHT_EXCEPTION_MSG("Unable to start receiving layouts.");
            }
        }

        // Listening comes after this, never before. A program that listens with no rule of its own
        // makes Windows show its own "allow access" alert, which lets it in on public networks too,
        // from any address. So Glass listens only once the firewall already lets it in on the
        // private network this PC is on, and otherwise offers its own narrower rule first.
        winrt::fire_and_forget ReceiveScreen::CheckNetworkAsync()
        {
            auto self = shared_from_this();

            try
            {
                if (m_checking)
                {
                    co_return;
                }

                m_checking = true;

                co_await winrt::resume_background();

                auto const state = firewall::QueryState(FirewallTarget());

                co_await wil::resume_foreground(m_dispatcher);

                m_checking = false;

                if (m_closed)
                {
                    co_return;
                }

                auto anyPrivate = false;
                auto privateOpen = false;
                auto blockAll = false;
                auto blockedByRule = false;

                for (auto const& network : state.Networks)
                {
                    if (network.Type == firewall::PublicNetworks)
                    {
                        continue;
                    }

                    anyPrivate = true;
                    privateOpen = privateOpen || LetsIn(network.Status);
                    blockAll = blockAll || network.Status == firewall::FirewallStatus::ClosedForAll;
                    blockedByRule = blockedByRule || network.Status == firewall::FirewallStatus::ClosedByRule;
                }

                m_allow.Visibility(xaml::Visibility::Collapsed);
                m_networkSettings.Visibility(xaml::Visibility::Collapsed);
                m_network.Foreground(::midiglass::sharing::ThemeBrush(L"TextFillColorPrimaryBrush"));

                auto const caution = [this]() { m_network.Foreground(::midiglass::sharing::ThemeBrush(L"SystemFillColorCautionBrush")); };

                // The firewall couldn't be read. Listening may bring up Windows' own alert, but
                // that's better than never receiving anything.
                auto listen = state.Status == firewall::FirewallStatus::Unknown;

                if (state.Status == firewall::FirewallStatus::NotConnected)
                {
                    m_network.Text(resources::GetString(L"ReceiveNotConnected"));
                }
                else if (state.Status == firewall::FirewallStatus::Unknown)
                {
                    m_network.Text(resources::GetString(L"ReceiveNetworkUnknown"));
                }
                else if (!anyPrivate)
                {
                    // Windows keeps other PCs out on a public network, and this app doesn't ask
                    // to change that.
                    m_network.Text(resources::GetString(L"ReceivePublicNetwork"));
                    caution();
                    m_networkSettings.Visibility(xaml::Visibility::Visible);
                }
                else if (privateOpen)
                {
                    m_network.Text(resources::GetString(L"ReceiveNetworkOk"));
                    listen = true;
                }
                else if (blockAll)
                {
                    m_network.Text(resources::GetString(L"ReceiveFirewallBlockAll"));
                    caution();
                }
                else if (blockedByRule)
                {
                    std::wstring names{};

                    for (auto const& name : state.BlockingRuleNames)
                    {
                        names += names.empty() ? L"" : L", ";
                        names += L"\u201C" + midiapp::SanitizeProvenanceText(name, 120) + L"\u201D";
                    }

                    m_network.Text(resources::FormatString(L"ReceiveFirewallBlockedByRuleFormat", names));
                    caution();
                }
                else if (state.ManagedByPolicy)
                {
                    m_network.Text(resources::GetString(L"ReceiveFirewallManaged"));
                    caution();
                }
                else
                {
                    m_network.Text(resources::GetString(L"ReceiveFirewallBlocked"));
                    caution();
                    m_allow.Visibility(xaml::Visibility::Visible);
                }

                if (listen)
                {
                    m_recheck.Stop();

                    if (m_receiver == nullptr && !m_starting)
                    {
                        StartAsync();
                    }
                }
                else if (m_receiver == nullptr)
                {
                    m_status.Text(resources::GetString(L"ReceiveNotListening"));

                    // A network set to private in Settings, or a rule turned off, is picked up
                    // without reopening the window.
                    m_recheck.Start();
                }
            }
            catch (...)
            {
                m_checking = false;
                LOG_CAUGHT_EXCEPTION_MSG("Unable to check the network for receiving.");
            }
        }

        winrt::fire_and_forget ReceiveScreen::AllowAsync()
        {
            auto self = shared_from_this();

            try
            {
                m_allow.IsEnabled(false);

                auto const owner = WindowHandle();

                co_await winrt::resume_background();

                // Private networks only (and a domain network the PC is on now), and only from the
                // local subnet (FirewallTarget).
                auto const connected = firewall::QueryState(FirewallTarget()).ConnectedNetworks;
                auto const result = firewall::RequestAllow(owner, FirewallTarget(),
                    firewall::PrivateNetworks | (connected & firewall::DomainNetworks));

                co_await wil::resume_foreground(m_dispatcher);

                if (m_closed)
                {
                    co_return;
                }

                m_allow.IsEnabled(true);

                if (FAILED(result) && result != HRESULT_FROM_WIN32(ERROR_CANCELLED))
                {
                    Log(resources::GetString(L"ReceiveAllowFailed"));
                }

                CheckNetworkAsync();
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to let Windows MIDI Glass through Windows Firewall.")
        }

        // Pairing is possible only while this dialog is open.
        winrt::fire_and_forget ReceiveScreen::PairAsync()
        {
            auto self = shared_from_this();

            try
            {
                if (m_pairingDialog != nullptr || m_importing || m_receiver == nullptr)
                {
                    co_return;
                }

                m_pairingDialog = controls::ContentDialog{};
                m_pairingDialog.XamlRoot(m_window.Content().XamlRoot());
                m_pairingDialog.Title(box_value(resources::GetString(L"ReceivePairTitle")));

                controls::StackPanel panel{};
                panel.Spacing(12.0);
                panel.Width(DialogWidth);

                controls::ProgressRing ring{};
                ring.IsActive(true);
                ring.Width(20.0);
                ring.Height(20.0);
                ring.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                automation::AutomationProperties::SetName(ring, resources::GetString(L"ReceivePairWaitingName"));

                panel.Children().Append(ring);
                panel.Children().Append(Paragraph(resources::FormatString(L"ReceivePairWaitingFormat", m_ownName), 14.0));

                m_pairingDialog.Content(panel);
                m_pairingDialog.CloseButtonText(resources::GetString(L"DialogCancel"));

                m_bridge->PairingOpen = true;

                auto const answer = co_await m_pairingDialog.ShowAsync();

                m_bridge->PairingOpen = false;
                m_pairingDialog = nullptr;

                if (m_pending != nullptr)
                {
                    m_pending->Answer(answer == controls::ContentDialogResult::Primary);
                    m_pending = nullptr;
                }

                ProcessQueueAsync();
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to pair with another PC.")
        }

        _Use_decl_annotations_
        void ReceiveScreen::ShowCode(std::shared_ptr<glass::sync::PendingPairing> const& pending)
        {
            // The dialog was closed in the meantime: that's a no.
            if (m_pairingDialog == nullptr || m_pending != nullptr)
            {
                pending->Answer(false);
                return;
            }

            m_pending = pending;

            controls::TextBlock code{};
            code.Text(pending->Code);
            code.FontSize(40.0);
            code.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            code.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            automation::AutomationProperties::SetName(code, resources::FormatString(L"SyncPairCodeNameFormat", pending->Code));

            controls::StackPanel panel{};
            panel.Spacing(14.0);
            panel.Width(DialogWidth);
            panel.Children().Append(Paragraph(resources::FormatString(L"SyncPairCompareFormat", pending->PeerName), 14.0));
            panel.Children().Append(code);
            panel.Children().Append(Paragraph(resources::GetString(L"SyncPairWhy"), 12.0, L"TextFillColorSecondaryBrush"));

            m_pairingDialog.Title(box_value(resources::FormatString(L"SyncPairTitleFormat", pending->PeerName)));
            m_pairingDialog.Content(panel);
            m_pairingDialog.PrimaryButtonText(resources::GetString(L"SyncPairMatch"));
            m_pairingDialog.CloseButtonText(resources::GetString(L"SyncPairNoMatch"));
        }

        // One import dialog at a time, and never while pairing is open: a window shows one
        // ContentDialog at a time.
        winrt::fire_and_forget ReceiveScreen::ProcessQueueAsync()
        {
            auto self = shared_from_this();

            try
            {
                if (m_importing || m_pairingDialog != nullptr || m_queue.empty() || m_closed)
                {
                    co_return;
                }

                m_importing = true;
                m_pair.IsEnabled(false);

                while (!m_queue.empty() && !m_closed)
                {
                    auto item = std::move(m_queue.front());
                    m_queue.pop_front();

                    co_await ImportAsync(std::move(item));
                }

                m_importing = false;

                if (!m_closed)
                {
                    m_pair.IsEnabled(m_receiver != nullptr);
                    m_status.Text(resources::GetString(L"ReceiveWaiting"));
                }
            }
            catch (...)
            {
                m_importing = false;
                LOG_CAUGHT_EXCEPTION_MSG("Unable to take a layout from another PC.");
            }
        }

        foundation::IAsyncAction ReceiveScreen::ImportAsync(glass::sync::ReceivedSync item)
        {
            auto self = shared_from_this();

            auto const sender = midiapp::DeviceKeyHashToHex(item.Sender);
            auto const senderName = item.SenderName;
            auto const layoutName = item.Offer.LayoutName;
            auto const layoutKey = glass::sync::SyncLayoutKey(item.Offer.LayoutId, item.Pack.Manifest.Primary);
            auto const received = item.FilesReceived;
            auto const reused = item.FilesReused;

            co_await winrt::resume_background();

            glass::sync::SyncRecords records{ DataFile(L"SyncRecords.json") };

            glass::PackSummary summary{};
            glass::SummarizePack(item.Pack, summary);

            auto existing = summary.ExistingPath;
            std::wstring existingOverride{};

            // A layout without an id is matched to the file the same PC sent before.
            if (existing.empty() && item.Offer.LayoutId.empty())
            {
                if (auto const record = records.Find(layoutKey, sender); record)
                {
                    std::error_code ec{};

                    if (std::filesystem::is_regular_file(record->InstalledPath, ec))
                    {
                        existingOverride = record->InstalledPath;
                        existing = existingOverride;
                    }
                }
            }

            auto const changedHere = glass::sync::CheckLocalCopy(records, layoutKey, sender, existing) == glass::sync::LocalCopyState::ChangedHere;
            auto const approved = glass::sync::SessionApprovals::Current().IsApproved(layoutKey, sender);

            co_await wil::resume_foreground(m_dispatcher);

            if (m_closed)
            {
                co_return;
            }

            m_status.Text(resources::FormatString(L"ReceiveCheckingFormat", layoutName, senderName));

            ::midiglass::sharing::PackImportOptions options{};
            options.FromDevice = senderName;
            options.AlreadyApproved = approved;
            options.ChangedHere = changedHere;
            options.ExistingPathOverride = existingOverride;
            options.Quiet = true;
            options.Approved = [layoutKey, sender]()
                {
                    glass::sync::SessionApprovals::Current().Approve(layoutKey, sender);
                };

            auto installedPath = std::make_shared<std::wstring>();

            co_await ::midiglass::sharing::ImportOpenedPackAsync(
                m_window.Content().XamlRoot(),
                std::move(item.Pack),
                options,
                [installedPath](::midiglass::sharing::ImportOutcome outcome, std::wstring const& path)
                {
                    if (outcome == ::midiglass::sharing::ImportOutcome::Layout)
                    {
                        *installedPath = path;
                    }
                });

            if (installedPath->empty())
            {
                if (!m_closed)
                {
                    Log(resources::FormatString(L"ReceiveDeclinedFormat", layoutName, senderName));
                }

                co_return;
            }

            auto const path = *installedPath;

            co_await winrt::resume_background();

            glass::sync::SyncRecord record{};
            record.LayoutKey = layoutKey;
            record.SenderKey = sender;
            record.InstalledPath = path;
            record.InstalledSha256 = glass::sync::HashFileSha256(path, midiapp::MaximumContentPackBytes);
            records.Remember(record);

            co_await wil::resume_foreground(m_dispatcher);

            if (m_installed)
            {
                m_installed(path);
            }

            // A window running this layout offers to load the new copy; it never swaps by itself.
            winrt::midiglass::implementation::App::NotifyLayoutUpdated(path);

            if (!m_closed)
            {
                Log(reused > 0
                    ? resources::FormatString(L"ReceiveGotDeltaFormat", layoutName, senderName, std::to_wstring(received), std::to_wstring(reused))
                    : resources::FormatString(L"ReceiveGotFormat", layoutName, senderName, std::to_wstring(received)));
            }
        }

        void ReceiveScreen::Stop() noexcept
        {
            m_closed = true;

            try
            {
                if (m_recheck != nullptr)
                {
                    m_recheck.Stop();
                }
            }
            catch (...)
            {
            }
            m_bridge->PairingOpen = false;

            if (m_pending != nullptr)
            {
                m_pending->Answer(false);
                m_pending = nullptr;
            }

            m_queue.clear();

            auto receiver = std::move(m_receiver);
            auto advertiser = std::move(m_advertiser);

            if (receiver == nullptr && advertiser == nullptr)
            {
                return;
            }

            // Withdrawing the advertisement waits on the DNS client, so it isn't done on the UI
            // thread. Listening stops first.
            try
            {
                std::thread([receiver, advertiser]()
                    {
                        if (receiver != nullptr)
                        {
                            receiver->Stop();
                        }

                        if (advertiser != nullptr)
                        {
                            advertiser->Stop();
                        }
                    }).detach();
            }
            catch (...)
            {
                if (receiver != nullptr)
                {
                    receiver->Stop();
                }

                if (advertiser != nullptr)
                {
                    advertiser->Stop();
                }
            }
        }
    }

    _Use_decl_annotations_
    void OpenReceiveWindow(std::function<void(std::wstring const&)> installed)
    {
        try
        {
            if (g_screen != nullptr)
            {
                g_screen->Activate();
                return;
            }

            g_screen = std::make_shared<ReceiveScreen>();
            g_screen->Open(std::move(installed));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the Receive screen.")
    }
}
