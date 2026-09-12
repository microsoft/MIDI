// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::midiclock;
namespace res = ::midiclock::resources;

namespace winrt::midiclock::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 980;
        constexpr int32_t DefaultWindowHeight = 660;

        implementation::ClockItem* Impl(winrt::midiclock::ClockItem const& item) noexcept
        {
            return item == nullptr ? nullptr : winrt::get_self<implementation::ClockItem>(item);
        }
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();

        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(
            *this, native::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            InitializeStaticText();
            InitializeWindowChrome();

            m_items = winrt::single_threaded_observable_vector<winrt::midiclock::ClockItem>();
            m_endpoints = winrt::single_threaded_observable_vector<appshared::EndpointChoice>();
            m_groups = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

            ClockRepeater().ItemsSource(m_items);
            EditEndpointComboBox().ItemsSource(m_endpoints);
            EditGroupComboBox().ItemsSource(m_groups);

            EditTempoNumberBox().Minimum(native::MinimumBeatsPerMinute);
            EditTempoNumberBox().Maximum(native::MaximumBeatsPerMinute);
            EditTempoSlider().Minimum(native::MinimumBeatsPerMinute);
            EditTempoSlider().Maximum(native::MaximumBeatsPerMinute);

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->StopEndpointWatcher();

                        // Synchronous on purpose: the stop messages and the pulses already in
                        // the service queue have to go out before this process ends, or an
                        // instrument is left running with nothing driving it.
                        strong->m_engine.StopAll();

                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_initialized = true;

            RebuildTiles();
            ReportStoreError();

            StartEndpointWatcher();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeStaticText() noexcept
    {
        try
        {
            Title(res::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(res::GetString(L"AppDisplayName"));

            EditDialog().PrimaryButtonText(res::GetString(L"EditDialogSave"));
            EditDialog().SecondaryButtonText(res::GetString(L"EditDialogDelete"));
            EditDialog().CloseButtonText(res::GetString(L"EditDialogCancel"));
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to set the window text.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().AlwaysOnTop(
                AlwaysOnTopToggle().IsChecked().GetBoolean());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSettingsToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const anchor = sender.try_as<xaml::FrameworkElement>();

            if (anchor == nullptr)
            {
                return;
            }

            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"AppearanceTitle");
            strings.ThemeLabel = res::GetString(L"AppearanceTheme");
            strings.ThemeSystem = res::GetString(L"AppearanceThemeSystem");
            strings.ThemeLight = res::GetString(L"AppearanceThemeLight");
            strings.ThemeDark = res::GetString(L"AppearanceThemeDark");
            strings.BackdropLabel = res::GetString(L"AppearanceBackdrop");
            strings.BackdropSolid = res::GetString(L"AppearanceBackdropSolid");
            strings.BackdropMica = res::GetString(L"AppearanceBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"AppearanceBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"AppearanceCustomColor");
            strings.ColorPickerName = res::GetString(L"AppearanceColorPickerName");

            midiapp::ShowAppearanceFlyout(
                anchor,
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to show the appearance settings.")
    }

    // ------------------------------------------------------------------------------------
    // Endpoints
    // ------------------------------------------------------------------------------------

    void MainWindow::StartEndpointWatcher() noexcept
    {
        try
        {
            m_watcher = midi2enum::MidiEndpointDeviceWatcher::Create(
                midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

            if (m_watcher == nullptr)
            {
                return;
            }

            auto const refresh = [weak = get_weak(), queue = m_dispatcherQueue]()
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->RefreshEndpointList();
                            }
                        });
                };

            m_watcherAddedToken = m_watcher.Added([refresh](auto&&, auto&&) { refresh(); });
            m_watcherRemovedToken = m_watcher.Removed([refresh](auto&&, auto&&) { refresh(); });
            m_watcherUpdatedToken = m_watcher.Updated([refresh](auto&&, auto&&) { refresh(); });

            m_watcher.Start();

            RefreshEndpointList();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to start the endpoint device watcher.")
    }

    void MainWindow::StopEndpointWatcher() noexcept
    {
        try
        {
            if (m_watcher == nullptr)
            {
                return;
            }

            m_watcher.Added(m_watcherAddedToken);
            m_watcher.Removed(m_watcherRemovedToken);
            m_watcher.Updated(m_watcherUpdatedToken);

            m_watcher.Stop();
            m_watcher = nullptr;
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to stop the endpoint device watcher.")
    }

    void MainWindow::RefreshEndpointList() noexcept
    {
        try
        {
            if (m_watcher == nullptr || m_endpoints == nullptr)
            {
                return;
            }

            // rebuilding the list under the customer would reset the picker they are using
            if (m_editorOpen)
            {
                return;
            }

            auto const devices = midiapp::SortedEndpoints(m_watcher);

            m_endpoints.Clear();
            m_endpointDevices.clear();

            for (auto const& device : devices)
            {
                winrt::hstring imagePath{};

                if (auto const userInfo = device.GetUserSuppliedInfo())
                {
                    imagePath = midiapp::ResolveEndpointImagePath(userInfo.ImageFileName());
                }

                m_endpoints.Append(winrt::make<appshared::implementation::EndpointChoice>(
                    device.Name(), device.EndpointDeviceId(), imagePath));

                m_endpointDevices.push_back(device);
            }

            RefreshTileText();
            ApplyStartupOptions();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to refresh the endpoint list.")
    }

    _Use_decl_annotations_
    std::optional<midi2enum::MidiEndpointDeviceInformation> MainWindow::FindEndpointDevice(
        std::wstring const& endpointDeviceId) const noexcept
    {
        try
        {
            if (endpointDeviceId.empty())
            {
                return std::nullopt;
            }

            for (auto const& device : m_endpointDevices)
            {
                if (midiapp::EndpointIdsMatch(device.EndpointDeviceId(), winrt::hstring{ endpointDeviceId }))
                {
                    return device;
                }
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> MainWindow::ResolveGroupIndexes(native::ClockDefinition const& definition) const noexcept
    {
        std::vector<uint8_t> groups{};

        try
        {
            if (definition.GroupIndex != native::AllDeclaredGroups)
            {
                groups.push_back(static_cast<uint8_t>(std::clamp(definition.GroupIndex, 0, 15)));
                return groups;
            }

            auto const device = FindEndpointDevice(definition.EndpointDeviceId);

            if (!device.has_value())
            {
                return groups;
            }

            auto const declared = midiapp::DeclaredGroups(device.value());

            for (uint8_t index = 0; index < 16; index++)
            {
                if (declared[index])
                {
                    groups.push_back(index);
                }
            }
        }
        catch (...)
        {
        }

        return groups;
    }

    // ------------------------------------------------------------------------------------
    // Tiles
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::midiclock::ClockItem MainWindow::ItemFromSender(foundation::IInspectable const& sender) const noexcept
    {
        try
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();

            if (element == nullptr)
            {
                return nullptr;
            }

            // ItemsRepeater with compiled bindings does not give the realized element a data
            // context, so the tile carries the id it stands for instead.
            if (auto const tag = element.Tag().try_as<winrt::hstring>())
            {
                return ItemFromId(std::wstring{ tag.value() });
            }

            return element.DataContext().try_as<winrt::midiclock::ClockItem>();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    winrt::midiclock::ClockItem MainWindow::ItemFromId(std::wstring const& id) const noexcept
    {
        try
        {
            if (m_items == nullptr || id.empty())
            {
                return nullptr;
            }

            for (uint32_t index = 0; index < m_items.Size(); index++)
            {
                auto const item = m_items.GetAt(index);

                if (item != nullptr && item.Id() == winrt::hstring{ id })
                {
                    return item;
                }
            }
        }
        catch (...)
        {
        }

        return nullptr;
    }

    void MainWindow::RebuildTiles() noexcept
    {
        try
        {
            if (m_items == nullptr)
            {
                return;
            }

            std::map<std::wstring, winrt::midiclock::ClockItem> existing{};

            for (uint32_t index = 0; index < m_items.Size(); index++)
            {
                auto const item = m_items.GetAt(index);

                if (item != nullptr)
                {
                    existing[std::wstring{ item.Id() }] = item;
                }
            }

            m_items.Clear();

            for (auto const& definition : native::ClockStore::Current().Clocks())
            {
                auto const found = existing.find(definition.Id);

                auto item = found != existing.end() ? found->second : winrt::make<implementation::ClockItem>();

                m_items.Append(item);
            }

            RefreshTileText();
            UpdateEmptyState();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to rebuild the clock tiles.")
    }

    void MainWindow::RefreshTileText() noexcept
    {
        try
        {
            if (m_items == nullptr)
            {
                return;
            }

            auto const& clocks = native::ClockStore::Current().Clocks();

            for (uint32_t index = 0; index < m_items.Size() && index < clocks.size(); index++)
            {
                auto const item = m_items.GetAt(index);
                auto const impl = Impl(item);

                if (impl == nullptr)
                {
                    continue;
                }

                auto const& definition = clocks[index];

                auto const device = FindEndpointDevice(definition.EndpointDeviceId);

                native::ClockRowData data{};

                data.Id = definition.Id;
                data.BeatsPerMinute = definition.BeatsPerMinute;
                data.GroupIndex = definition.GroupIndex;

                data.EndpointName = device.has_value()
                    ? std::wstring{ device.value().Name() }
                    : definition.EndpointName;

                data.IsEndpointMissing = !definition.EndpointDeviceId.empty() && !device.has_value();

                data.DisplayName = !definition.Name.empty()
                    ? definition.Name
                    : (!data.EndpointName.empty()
                        ? data.EndpointName
                        : std::wstring{ res::GetString(L"TileUnnamedClock") });

                impl->Update(data);
                impl->IsRunning(m_engine.IsRunning(definition.Id));
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to refresh the clock tiles.")
    }

    void MainWindow::UpdateEmptyState() noexcept
    {
        try
        {
            auto const isEmpty = m_items == nullptr || m_items.Size() == 0;

            EmptyStatePanel().Visibility(isEmpty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            TileScroller().Visibility(isEmpty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            StartAllButton().IsEnabled(!isEmpty);
            StartSelectedButton().IsEnabled(!isEmpty);
            StopAllButton().IsEnabled(!isEmpty);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update the empty state.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateStatus(winrt::hstring const& message) noexcept
    {
        try
        {
            StatusTextBlock().Text(message);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to set the status text.")
    }

    void MainWindow::ReportStoreError() noexcept
    {
        try
        {
            auto const error = native::ClockStore::Current().LastErrorMessage();

            if (!error.empty())
            {
                UpdateStatus(error);
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to report a clock store error.")
    }

    void MainWindow::ApplyStartupOptions() noexcept
    {
        try
        {
            if (m_startupOptionsApplied)
            {
                return;
            }

            auto const& options = App::StartupOptions();

            if (options.HasError || options.ShowHelp)
            {
                m_startupOptionsApplied = true;

                if (options.HasError)
                {
                    UpdateStatus(res::FormatString(
                        options.ErrorResourceKey, options.ErrorArgument));
                }

                return;
            }

            if (options.EndpointDeviceId.empty())
            {
                m_startupOptionsApplied = true;

                if (options.Start)
                {
                    StartClocksAsync(AllClockIds());
                }

                return;
            }

            // wait for the first enumeration pass, which reports no devices at all
            if (m_endpointDevices.empty())
            {
                return;
            }

            auto const device = FindEndpointDevice(options.EndpointDeviceId);

            if (!device.has_value())
            {
                return;
            }

            m_startupOptionsApplied = true;

            auto& store = native::ClockStore::Current();

            auto const existing = store.FindByEndpoint(options.EndpointDeviceId);

            native::ClockDefinition definition{};

            if (existing != nullptr)
            {
                definition = *existing;
            }
            else
            {
                definition.EndpointDeviceId = options.EndpointDeviceId;
                definition.GroupIndex = 0;
            }

            definition.EndpointName = std::wstring{ device.value().Name() };

            if (options.GroupNumber.has_value())
            {
                definition.GroupIndex = static_cast<int32_t>(options.GroupNumber.value()) - 1;
            }

            if (options.BeatsPerMinute.has_value())
            {
                definition.BeatsPerMinute = options.BeatsPerMinute.value();
            }

            auto const id = store.Upsert(definition);

            if (id.empty())
            {
                ReportStoreError();
                return;
            }

            store.Save();

            RebuildTiles();

            if (options.Start)
            {
                StartClocksAsync({ id });
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to apply the command line options.")
    }

    // ------------------------------------------------------------------------------------
    // Commands
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnAddClockClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (native::ClockStore::Current().IsFull())
            {
                UpdateStatus(res::GetString(L"ClockStoreFullError"));
                return;
            }

            ShowEditorAsync({});
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to add a clock.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTileTapped(foundation::IInspectable const& sender, input::TappedRoutedEventArgs const& args)
    {
        try
        {
            auto const item = ItemFromSender(sender);

            if (item == nullptr)
            {
                return;
            }

            args.Handled(true);

            ShowEditorAsync(std::wstring{ item.Id() });
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to open the clock editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTileEditClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = ItemFromSender(sender);

            if (item != nullptr)
            {
                ShowEditorAsync(std::wstring{ item.Id() });
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to open the clock editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTileStartStopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = ItemFromSender(sender);

            if (item == nullptr)
            {
                return;
            }

            std::vector<std::wstring> ids{ std::wstring{ item.Id() } };

            if (item.IsRunning())
            {
                StopClocksAsync(std::move(ids));
            }
            else
            {
                StartClocksAsync(std::move(ids));
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to start or stop a clock.")
    }

    std::vector<std::wstring> MainWindow::AllClockIds() const noexcept
    {
        std::vector<std::wstring> ids{};

        try
        {
            for (auto const& definition : native::ClockStore::Current().Clocks())
            {
                ids.push_back(definition.Id);
            }
        }
        catch (...)
        {
        }

        return ids;
    }

    _Use_decl_annotations_
    void MainWindow::OnStartAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            StartClocksAsync(AllClockIds());
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to start the clocks.")
    }

    _Use_decl_annotations_
    void MainWindow::OnStartSelectedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            std::vector<std::wstring> ids{};

            if (m_items != nullptr)
            {
                for (uint32_t index = 0; index < m_items.Size(); index++)
                {
                    auto const item = m_items.GetAt(index);

                    if (item != nullptr && item.IsSelected())
                    {
                        ids.push_back(std::wstring{ item.Id() });
                    }
                }
            }

            if (ids.empty())
            {
                UpdateStatus(res::GetString(L"StatusNothingSelected"));
                return;
            }

            StartClocksAsync(std::move(ids));
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to start the selected clocks.")
    }

    _Use_decl_annotations_
    void MainWindow::OnStopAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            StopClocksAsync(m_engine.RunningIds());
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to stop the clocks.")
    }

    _Use_decl_annotations_
    void MainWindow::SetBusy(std::vector<std::wstring> const& ids, bool isBusy) noexcept
    {
        try
        {
            for (auto const& id : ids)
            {
                if (auto const impl = Impl(ItemFromId(id)))
                {
                    impl->IsBusy(isBusy);
                }
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update the busy state.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyStartResults(
        std::map<std::wstring, native::ClockStartResult> const& results) noexcept
    {
        try
        {
            size_t started{ 0 };

            std::wstring_view failureKey{};

            for (auto const& result : results)
            {
                if (result.second == native::ClockStartResult::Success)
                {
                    started++;
                    continue;
                }

                switch (result.second)
                {
                case native::ClockStartResult::ServiceUnavailable:
                    failureKey = L"StatusServiceUnavailable";
                    break;

                case native::ClockStartResult::SessionFailed:
                    failureKey = L"StatusSessionFailed";
                    break;

                case native::ClockStartResult::NoEndpointChosen:
                    failureKey = L"StatusNoEndpointChosen";
                    break;

                default:
                    failureKey = L"StatusConnectionFailed";
                    break;
                }
            }

            RefreshTileText();

            if (!failureKey.empty())
            {
                UpdateStatus(res::GetString(failureKey));
            }
            else if (started > 0)
            {
                UpdateStatus(res::FormatString(L"StatusStartedFormat", started));
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to report the start results.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::StartClocksAsync(std::vector<std::wstring> ids)
    {
        auto strong = get_strong();

        try
        {
            if (ids.empty())
            {
                co_return;
            }

            auto requests = std::make_shared<std::vector<native::ClockStartRequest>>();

            for (auto const& id : ids)
            {
                auto const definition = native::ClockStore::Current().Find(id);

                if (definition == nullptr)
                {
                    continue;
                }

                native::ClockStartRequest request{};

                request.Id = definition->Id;
                request.EndpointDeviceId = definition->EndpointDeviceId;
                request.BeatsPerMinute = definition->BeatsPerMinute;
                request.PulsesPerQuarterNote = definition->PulsesPerQuarterNote;
                request.SendStartStop = definition->SendStartStop;
                request.GroupIndexes = ResolveGroupIndexes(*definition);

                requests->push_back(std::move(request));
            }

            if (requests->empty())
            {
                co_return;
            }

            SetBusy(ids, true);
            UpdateStatus(res::GetString(L"StatusStarting"));

            auto results = std::make_shared<std::map<std::wstring, native::ClockStartResult>>();

            co_await native::RunOnBackgroundAsync([strong, requests, results]()
                {
                    *results = strong->m_engine.Start(*requests);
                });

            SetBusy(ids, false);
            ApplyStartResults(*results);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to start the clocks.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::StopClocksAsync(std::vector<std::wstring> ids)
    {
        auto strong = get_strong();

        try
        {
            if (ids.empty())
            {
                co_return;
            }

            SetBusy(ids, true);
            UpdateStatus(res::GetString(L"StatusStopping"));

            auto const work = std::make_shared<std::vector<std::wstring>>(ids);

            co_await native::RunOnBackgroundAsync([strong, work]()
                {
                    for (auto const& id : *work)
                    {
                        strong->m_engine.Stop(id);
                    }
                });

            SetBusy(ids, false);
            RefreshTileText();

            UpdateStatus(res::GetString(L"StatusStopped"));
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to stop the clocks.")
    }
}
