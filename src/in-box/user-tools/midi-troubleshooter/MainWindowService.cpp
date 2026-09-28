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
        winrt::hstring GuidText(_In_ winrt::guid const& value) noexcept
        {
            try
            {
                return winrt::to_hstring(value);
            }
            catch (...)
            {
                return {};
            }
        }

        winrt::hstring Lowered(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                std::transform(copy.begin(), copy.end(), copy.begin(),
                    [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                return winrt::hstring{ copy };
            }
            catch (...)
            {
                return value;
            }
        }

        // Registry keys and the service both write braced GUIDs, but not always in the same
        // case, and a hand-edited entry may have no braces at all.
        winrt::hstring NormalizedGuid(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                if (value.empty())
                {
                    return {};
                }

                return GuidText(winrt::guid{ std::wstring_view{ value } });
            }
            catch (...)
            {
                return Lowered(value);
            }
        }

        winrt::hstring FormatLocalTime(_In_ foundation::DateTime const& value) noexcept
        {
            try
            {
                if (value.time_since_epoch().count() == 0)
                {
                    return {};
                }

                // the customer's own date and time format, and their time zone, rather than a
                // fixed pattern which reads as a foreign one nearly everywhere
                winrt::Windows::Globalization::DateTimeFormatting::DateTimeFormatter const formatter{ L"shortdate shorttime" };

                return formatter.Format(value);
            }
            catch (...)
            {
                return {};
            }
        }

        // A session reports only the device id it opened, so the readable name has to be looked up.
        // Falls back to the id for an endpoint which has gone away since the session opened it.
        winrt::hstring ConnectionDisplayName(_In_ winrt::hstring const& deviceId) noexcept
        {
            try
            {
                if (midi2enum::MidiEndpointDeviceHelper::IsPossibleWindowsMidiServicesEndpointDeviceId(deviceId))
                {
                    if (auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(deviceId))
                    {
                        if (!info.Name().empty())
                        {
                            return info.Name();
                        }
                    }
                }
                else if (midi2enum::MidiEndpointDeviceHelper::IsPossibleWindowsMidiServicesLegacyApiPortDeviceId(deviceId))
                {
                    if (auto const info = midi2legacy::MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(deviceId))
                    {
                        if (!info.Name().empty())
                        {
                            return info.Name();
                        }
                    }
                }
            }
            catch (...)
            {
            }

            return deviceId;
        }

        winrt::hstring ServiceStateText(_In_ native::ServiceState const state) noexcept
        {
            switch (state)
            {
            case native::ServiceState::Running:         return res::GetString(L"ServiceStateRunning");
            case native::ServiceState::Stopped:         return res::GetString(L"ServiceStateStopped");
            case native::ServiceState::Paused:          return res::GetString(L"ServiceStatePaused");
            case native::ServiceState::StartPending:    return res::GetString(L"ServiceStateStarting");
            case native::ServiceState::StopPending:     return res::GetString(L"ServiceStateStopping");
            case native::ServiceState::PausePending:    return res::GetString(L"ServiceStatePausing");
            case native::ServiceState::ContinuePending: return res::GetString(L"ServiceStateResuming");
            case native::ServiceState::NotInstalled:    return res::GetString(L"ServiceStateNotInstalled");
            default:                                    return res::GetString(L"ServiceStateUnknown");
            }
        }

        winrt::hstring ServiceStartModeText(_In_ native::ServiceStartMode const mode) noexcept
        {
            switch (mode)
            {
            case native::ServiceStartMode::Automatic:           return res::GetString(L"ServiceStartAutomatic");
            case native::ServiceStartMode::AutomaticDelayed:    return res::GetString(L"ServiceStartAutomaticDelayed");
            case native::ServiceStartMode::Manual:              return res::GetString(L"ServiceStartManual");
            case native::ServiceStartMode::Disabled:            return res::GetString(L"ServiceStartDisabled");
            case native::ServiceStartMode::Boot:                return res::GetString(L"ServiceStartBoot");
            case native::ServiceStartMode::System:              return res::GetString(L"ServiceStartSystem");
            default:                                            return res::GetString(L"ServiceStateUnknown");
            }
        }

        // The transports that come with Windows MIDI Services, so each one can say what turning
        // it off takes away. Anything else gets the general warning.
        struct KnownTransport
        {
            wchar_t const* ClassId;
            wchar_t const* Code;
            wchar_t const* DisableWarningKey;
        };

        constexpr KnownTransport KnownTransports[]
        {
            { L"{0f273b18-e372-4d95-87ac-c31c3d22e937}", L"KSA",     L"TransportDisableWarningKsa" },
            { L"{26fa740d-469c-4d33-beb1-3885de7d6df1}", L"KS",      L"TransportDisableWarningKs" },
            { L"{942bf02d-93c0-4ea8-b03e-d51156ca75e1}", L"LOOP",    L"TransportDisableWarningLoopback" },
            { L"{10088473-9478-4e62-850b-3d2315e135b8}", L"BLOOP",   L"TransportDisableWarningBasicLoopback" },
            { L"{54c9b2f6-c235-4000-a675-9f6958a1a4fa}", L"RTPMIDI", L"TransportDisableWarningRtpMidi" },
            { L"{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}", L"NET2UDP", L"TransportDisableWarningNetwork" },
            { L"{5dc87270-f318-4838-a4f9-6aadc63e925f}", L"BLEMIDI", L"TransportDisableWarningBluetooth" },
            { L"{8feaad91-70e1-4a19-997a-377720a719c1}", L"APP",     L"TransportDisableWarningVirtual" },
            { L"{7605713e-fea9-409d-a90f-a81233200d0a}", L"GMSYNTH", L"TransportDisableWarningSynth" },
        };

        KnownTransport const* FindKnownTransport(_In_ winrt::hstring const& transportId) noexcept
        {
            for (auto const& known : KnownTransports)
            {
                if (_wcsicmp(known.ClassId, transportId.c_str()) == 0)
                {
                    return &known;
                }
            }

            return nullptr;
        }

        winrt::hstring DisableWarning(_In_ winrt::hstring const& transportId, _In_ winrt::hstring const& name) noexcept
        {
            if (auto const known = FindKnownTransport(transportId))
            {
                return res::GetString(known->DisableWarningKey);
            }

            return res::FormatString(L"TransportDisableWarningOtherFormat", name);
        }

        // The data context of a control inside a DataTemplate is the row it was realized for.
        miditroubleshooter::TransportItem TransportItemFromSender(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                if (element == nullptr)
                {
                    return nullptr;
                }

                return element.DataContext().try_as<miditroubleshooter::TransportItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }
    }

    MainWindow::ServiceSnapshot MainWindow::GatherServiceSnapshot() noexcept
    {
        ServiceSnapshot snapshot{};

        try
        {
            snapshot.Service = native::QueryMidiServiceStatus();
            snapshot.RegisteredTransports = native::ScanRegisteredTransports();

            // Asking the service anything while it is stopped would start it, which is not
            // something a troubleshooting tool should do behind the customer's back.
            if (snapshot.Service.State == native::ServiceState::Running)
            {
                snapshot.Sessions = midi2rept::MidiReporting::GetActiveSessions();
                snapshot.LoadedTransports = midi2rept::MidiReporting::GetInstalledTransportPlugins();
            }

            snapshot.Gathered = true;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to gather the service snapshot.")

        return snapshot;
    }

    winrt::fire_and_forget MainWindow::RequestServiceRefreshAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_closing || !m_loaded)
            {
                co_return;
            }

            // A tick is skipped rather than queued when the previous pass is still running.
            if (m_refreshInFlight.exchange(true))
            {
                co_return;
            }

            ServiceSnapshot snapshot{};

            co_await native::RunOnBackgroundAsync([&snapshot]()
                {
                    snapshot = GatherServiceSnapshot();
                });

            m_refreshInFlight = false;

            if (m_closing)
            {
                co_return;
            }

            ApplyServiceSnapshot(snapshot);
        }
        catch (...)
        {
            m_refreshInFlight = false;

            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to refresh the service state.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyServiceSnapshot(ServiceSnapshot const& snapshot) noexcept
    {
        if (!snapshot.Gathered)
        {
            return;
        }

        ApplyServiceStatus(snapshot);
        ApplySessions(snapshot);
        ApplyTransports(snapshot);
    }

    _Use_decl_annotations_
    void MainWindow::ApplyServiceStatus(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            auto const& service = snapshot.Service;

            ServiceStateValue().Text(ServiceStateText(service.State));
            ServiceStartModeValue().Text(ServiceStartModeText(service.StartMode));

            ServiceAccountValue().Text(service.AccountName.empty() ?
                res::GetString(L"ValueUnknown") : winrt::hstring{ service.AccountName });

            ServiceProcessValue().Text(service.ProcessId == 0 ?
                res::GetString(L"ServiceNoProcess") :
                res::FormatString(L"ServiceProcessFormat", static_cast<uint32_t>(service.ProcessId)));

            ServiceImageValue().Text(service.ImagePath.empty() ?
                res::GetString(L"ValueUnknown") : winrt::hstring{ service.ImagePath });

            ServiceVersionValue().Text(service.ImageVersion.empty() ?
                res::GetString(L"ValueUnknown") : winrt::hstring{ service.ImageVersion });

            RestartServiceButton().IsEnabled(service.Installed && !m_busy);

            SetAutomaticStartButton().IsEnabled(
                service.Installed && service.StartMode != native::ServiceStartMode::Automatic);

            SetManualStartButton().IsEnabled(
                service.Installed && service.StartMode != native::ServiceStartMode::Manual);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the service status.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplySessions(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            if (snapshot.Sessions == nullptr)
            {
                m_sessions.Clear();

                SessionsStatusText().Text(snapshot.Service.State == native::ServiceState::Running ?
                    res::GetString(L"SessionsUnavailable") : res::GetString(L"SessionsServiceNotRunning"));

                return;
            }

            std::vector<winrt::hstring> seen{};

            for (auto const& session : snapshot.Sessions)
            {
                auto const sessionId = GuidText(session.SessionId());

                seen.push_back(sessionId);

                miditroubleshooter::SessionItem item{ nullptr };

                for (auto const& existing : m_sessions)
                {
                    if (existing.SessionId() == sessionId)
                    {
                        item = existing;
                        break;
                    }
                }

                if (item == nullptr)
                {
                    item = winrt::make<SessionItem>();
                    m_sessions.Append(item);
                }

                auto const connections = session.Connections();
                auto const connectionCount = connections == nullptr ? 0u : connections.Size();

                auto const name = session.SessionName();

                winrt::get_self<SessionItem>(item)->Update(
                    sessionId,
                    name.empty() ? res::GetString(L"SessionUnnamed") : name,
                    res::FormatString(L"SessionProcessFormat",
                        session.ProcessName(),
                        static_cast<uint64_t>(session.ProcessId())),
                    res::FormatString(L"SessionStartedFormat", FormatLocalTime(session.StartTime())),
                    res::FormatString(L"SessionConnectionCountFormat", connectionCount));

                auto const rows = item.Connections();

                // Rebuilt only when the shape changed, so an expanded session does not flicker
                // on every poll.
                if (rows.Size() != connectionCount)
                {
                    rows.Clear();

                    for (uint32_t i = 0; i < connectionCount; i++)
                    {
                        rows.Append(winrt::make<SessionConnectionItem>());
                    }
                }

                for (uint32_t i = 0; i < connectionCount; i++)
                {
                    auto const connection = connections.GetAt(i);
                    auto const deviceId = connection.EndpointOrPortDeviceId();

                    winrt::get_self<SessionConnectionItem>(rows.GetAt(i))->Update(
                        deviceId,
                        ConnectionDisplayName(deviceId),
                        res::FormatString(L"SessionConnectionInstancesFormat",
                            static_cast<uint32_t>(connection.InstanceCount())),
                        res::FormatString(L"SessionConnectionSinceFormat",
                            FormatLocalTime(connection.EarliestConnectionTime())));
                }
            }

            for (int32_t i = static_cast<int32_t>(m_sessions.Size()) - 1; i >= 0; i--)
            {
                auto const sessionId = m_sessions.GetAt(static_cast<uint32_t>(i)).SessionId();

                if (std::find(seen.begin(), seen.end(), sessionId) == seen.end())
                {
                    m_sessions.RemoveAt(static_cast<uint32_t>(i));
                }
            }

            SessionsStatusText().Text(m_sessions.Size() == 0 ?
                res::GetString(L"SessionsNone") :
                res::FormatString(L"SessionsCountFormat", m_sessions.Size()));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the active sessions.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyTransports(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            using Severity = TransportItem::Severity;

            auto const serviceRunning = snapshot.Service.State == native::ServiceState::Running;
            auto const serviceStartTime = snapshot.Service.ProcessStartTime;

            // A record belongs to one service process. A new one read the registry afresh.
            if (!serviceRunning ||
                snapshot.Service.ProcessId != m_transportRecordProcessId ||
                serviceStartTime != m_transportRecordStartTime)
            {
                m_transportLoadedAtServiceStart.clear();
                m_transportRecordProcessId = serviceRunning ? snapshot.Service.ProcessId : 0;
                m_transportRecordStartTime = serviceRunning ? serviceStartTime : 0;
            }

            // The service reads the registry again for this list, so it names what it would load
            // now, not what it loaded when it started.
            std::vector<winrt::hstring> reported{};

            if (snapshot.LoadedTransports != nullptr)
            {
                for (auto const& transport : snapshot.LoadedTransports)
                {
                    auto const key = NormalizedGuid(GuidText(transport.TransportId()));

                    reported.push_back(key);

                    m_transportDescriptions[std::wstring{ key }] = TransportDescription{
                        transport.Name(),
                        transport.TransportCode(),
                        transport.Description(),
                        res::FormatString(L"TransportDetailFormat",
                            transport.Version().empty() ? res::GetString(L"ValueUnknown") : transport.Version(),
                            transport.Author().empty() ? res::GetString(L"ValueUnknown") : transport.Author()) };
                }
            }

            auto const isReported = [&reported](winrt::hstring const& key)
                {
                    return !key.empty() && std::find(reported.begin(), reported.end(), key) != reported.end();
                };

            auto const setState = [](
                TransportItem::Values& row,
                std::wstring_view const stateKey,
                Severity const severity,
                std::wstring_view const reasonKey)
                {
                    row.State = res::GetString(stateKey);
                    row.StateSeverity = severity;
                    row.Reason = reasonKey.empty() ? winrt::hstring{} : res::GetString(reasonKey);
                };

            std::vector<TransportItem::Values> rows{};

            auto awaitingRestart = false;

            // In registry order, so a row stays where it is when its transport is switched off.
            for (auto const& registered : snapshot.RegisteredTransports)
            {
                TransportItem::Values row{};

                auto const key = NormalizedGuid(winrt::hstring{ registered.ClassId });
                auto const loadable = isReported(key);

                row.TransportId = key.empty() ? winrt::hstring{ registered.Name } : key;
                row.Name = winrt::hstring{ registered.Name };

                if (auto const known = FindKnownTransport(key))
                {
                    row.Code = known->Code;
                }

                if (!registered.ModuleVersion.empty())
                {
                    row.Detail = res::FormatString(
                        L"TransportVersionOnlyFormat", winrt::hstring{ registered.ModuleVersion });
                }

                if (!registered.ModulePath.empty())
                {
                    row.Module = res::FormatString(
                        L"TransportModuleFormat", winrt::hstring{ registered.ModulePath });
                }

                if (auto const described = m_transportDescriptions.find(std::wstring{ key });
                    !key.empty() && described != m_transportDescriptions.end())
                {
                    auto const& description = described->second;

                    if (!description.Name.empty())
                    {
                        row.Name = description.Name;
                    }

                    if (!description.Code.empty())
                    {
                        row.Code = description.Code;
                    }

                    row.Description = description.Description;
                    row.Detail = description.Detail;
                }

                if (registered.BuiltIn)
                {
                    if (!registered.ModuleRegistered)
                    {
                        setState(row, L"TransportStateEnabledNotLoaded", Severity::Error, L"TransportReasonNotRegistered");
                    }
                    else if (!registered.ModuleFileFound)
                    {
                        setState(row, L"TransportStateEnabledNotLoaded", Severity::Error, L"TransportReasonFileMissing");
                    }
                    else if (loadable)
                    {
                        row.Loaded = true;

                        setState(row, L"TransportStateEnabledLoaded", Severity::Ok, L"TransportReasonBuiltIn");
                    }
                    else
                    {
                        // The service hosts these itself and never reports them as plugins, so
                        // "did not load" would be wrong. A present registration is all there is
                        // to check.
                        setState(row, L"TransportStatusClientComponent", Severity::Ok, {});

                        row.Description = res::GetString(L"TransportClientComponentDescription");
                    }
                }
                else
                {
                    row.RegistryKeyName = winrt::hstring{ registered.KeyName };
                    row.Enabled = registered.Enabled;

                    if (serviceRunning)
                    {
                        auto const recorded = m_transportLoadedAtServiceStart.find(std::wstring{ row.TransportId });

                        if (recorded != m_transportLoadedAtServiceStart.end())
                        {
                            row.Loaded = recorded->second;
                        }
                        else if (serviceStartTime != 0 && registered.KeyLastWriteTime > serviceStartTime)
                        {
                            // written since the service read it, which in practice is someone
                            // switching it on or off
                            row.Loaded = !registered.Enabled;
                        }
                        else
                        {
                            row.Loaded = registered.Enabled && loadable;
                        }
                    }

                    if (registered.Enabled && row.Loaded)
                    {
                        setState(row, L"TransportStateEnabledLoaded", Severity::Ok, {});
                    }
                    else if (registered.Enabled)
                    {
                        if (!registered.ModuleRegistered)
                        {
                            setState(row, L"TransportStateEnabledNotLoaded", Severity::Error, L"TransportReasonNotRegistered");
                        }
                        else if (!registered.ModuleFileFound)
                        {
                            setState(row, L"TransportStateEnabledNotLoaded", Severity::Error, L"TransportReasonFileMissing");
                        }
                        else if (!serviceRunning)
                        {
                            setState(row, L"TransportStateEnabledNotLoaded", Severity::Warning, L"TransportReasonServiceNotRunning");
                        }
                        else if (loadable)
                        {
                            setState(row, L"TransportStateEnabledNotLoaded", Severity::Warning, L"TransportReasonLoadsAfterRestart");

                            awaitingRestart = true;
                        }
                        else
                        {
                            setState(row, L"TransportStateEnabledNotLoaded", Severity::Error, L"TransportReasonNotLoaded");
                        }
                    }
                    else if (row.Loaded)
                    {
                        setState(row, L"TransportStateDisabledLoaded", Severity::Warning, L"TransportReasonUnloadsAfterRestart");

                        awaitingRestart = true;
                    }
                    else
                    {
                        setState(row, L"TransportStateDisabledNotLoaded", Severity::Neutral, {});
                    }

                    row.ToggleText = res::GetString(
                        registered.Enabled ? L"TransportDisableButtonText" : L"TransportEnableButtonText");

                    row.ToggleAccessibleName = res::FormatString(
                        registered.Enabled ? L"TransportDisableButtonAccessibleNameFormat" : L"TransportEnableButtonAccessibleNameFormat",
                        row.Name);
                }

                row.RowAccessibleName = res::FormatString(L"TransportRowAccessibleNameFormat", row.Name, row.State);

                rows.push_back(row);
            }

            // Anything the service reported that the registry scan did not find is still shown.
            for (auto const& key : reported)
            {
                if (key.empty() ||
                    std::any_of(rows.begin(), rows.end(),
                        [&key](TransportItem::Values const& row) { return row.TransportId == key; }))
                {
                    continue;
                }

                auto const& description = m_transportDescriptions[std::wstring{ key }];

                TransportItem::Values row{};

                row.TransportId = key;
                row.Name = description.Name.empty() ? key : description.Name;
                row.Code = description.Code;
                row.Description = description.Description;
                row.Detail = description.Detail;
                row.Loaded = true;

                setState(row, L"TransportStateEnabledLoaded", Severity::Ok, {});

                row.RowAccessibleName = res::FormatString(L"TransportRowAccessibleNameFormat", row.Name, row.State);

                rows.push_back(row);
            }

            // Rows are addressed by index rather than matched, because the composition above
            // already produces a stable order and the list is short.
            while (m_transports.Size() > rows.size())
            {
                m_transports.RemoveAtEnd();
            }

            while (m_transports.Size() < rows.size())
            {
                m_transports.Append(winrt::make<TransportItem>());
            }

            for (uint32_t i = 0; i < rows.size(); i++)
            {
                winrt::get_self<TransportItem>(m_transports.GetAt(i))->Update(rows[i]);
            }

            auto const notLoaded = static_cast<uint32_t>(std::count_if(rows.begin(), rows.end(),
                [](TransportItem::Values const& row) { return row.StateSeverity != Severity::Ok; }));

            TransportsStatusText().Text(notLoaded == 0 ?
                res::FormatString(L"TransportsAllLoadedFormat", static_cast<uint32_t>(rows.size())) :
                res::FormatString(L"TransportsProblemFormat", static_cast<uint32_t>(rows.size()), notLoaded));

            TransportsRestartInfoBar().IsOpen(awaitingRestart);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the transports.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRefreshServiceHealthClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        RequestServiceRefreshAsync();

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRestartServiceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"ServiceRestartConfirmTitle"),
                res::GetString(L"ServiceRestartConfirmMessage"));

            if (!confirmed)
            {
                co_return;
            }

            m_busy = true;

            RestartServiceButton().IsEnabled(false);
            ServiceProgressRing().IsActive(true);
            ServiceStatusText().Text(res::GetString(L"ServiceRestarting"));

            // Runs on the closing and the exception paths too, so a restart that goes wrong
            // cannot leave the page spinning. The refresh is what puts the button back, since
            // only it knows whether the service is installed at all.
            auto const clearBusy = wil::scope_exit([this]() noexcept
                {
                    try
                    {
                        m_busy = false;

                        if (!m_closing)
                        {
                            ServiceProgressRing().IsActive(false);
                            RequestServiceRefreshAsync();
                        }
                    }
                    catch (...)
                    {
                    }
                });

            native::ServiceOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result]()
                {
                    result = native::RestartMidiService();
                });

            if (m_closing)
            {
                co_return;
            }

            ServiceStatusText().Text(result.Succeeded ?
                res::GetString(L"ServiceRestarted") :
                res::FormatString(L"ServiceRestartFailedFormat", winrt::hstring{ result.ErrorMessage }));

            if (result.Succeeded)
            {
                OnMidiServiceRestarted();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to restart the service.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSetAutomaticStartClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            native::ServiceOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result]()
                {
                    result = native::SetMidiServiceStartMode(native::ServiceStartMode::Automatic);
                });

            if (m_closing)
            {
                co_return;
            }

            ServiceStatusText().Text(result.Succeeded ?
                res::GetString(L"ServiceStartModeChanged") :
                res::FormatString(L"ServiceStartModeFailedFormat", winrt::hstring{ result.ErrorMessage }));

            RequestServiceRefreshAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the service to start automatically.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSetManualStartClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            native::ServiceOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result]()
                {
                    result = native::SetMidiServiceStartMode(native::ServiceStartMode::Manual);
                });

            if (m_closing)
            {
                co_return;
            }

            ServiceStatusText().Text(result.Succeeded ?
                res::GetString(L"ServiceStartModeChanged") :
                res::FormatString(L"ServiceStartModeFailedFormat", winrt::hstring{ result.ErrorMessage }));

            RequestServiceRefreshAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the service to start on demand.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnToggleTransportClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        auto const item = TransportItemFromSender(sender);

        if (item == nullptr || !item.CanToggle())
        {
            co_return;
        }

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const self = winrt::get_self<TransportItem>(item);

            // Taken before anything is awaited, because a refresh can hand this row to another
            // transport in the meantime.
            auto const transportId = std::wstring{ item.TransportId() };
            auto const name = item.Name();
            auto const keyName = std::wstring{ self->RegistryKeyName() };
            auto const enable = !self->TransportEnabled();
            auto const loadedNow = self->TransportLoaded();

            if (!enable)
            {
                auto const confirmed = co_await ConfirmAsync(
                    res::FormatString(L"TransportDisableConfirmTitleFormat", name),
                    DisableWarning(winrt::hstring{ transportId }, name));

                if (!confirmed)
                {
                    co_return;
                }
            }

            item.IsBusy(true);
            TransportsProgressRing().IsActive(true);

            // fires on the closing and the exception paths too, so the row cannot be left busy
            auto const clearBusy = wil::scope_exit([this, item]() noexcept
                {
                    try
                    {
                        item.IsBusy(false);

                        if (!m_closing)
                        {
                            TransportsProgressRing().IsActive(false);
                        }
                    }
                    catch (...)
                    {
                    }
                });

            // The running service keeps what it loaded at startup, and the registry is about to
            // stop saying what that was.
            if (m_transportRecordProcessId != 0)
            {
                m_transportLoadedAtServiceStart.try_emplace(transportId, loadedNow);
            }

            auto written = false;

            co_await native::RunOnBackgroundAsync([&written, &keyName, enable]()
                {
                    written = native::TrySetTransportEnabled(keyName, enable);
                });

            if (m_closing)
            {
                co_return;
            }

            TransportsActionText().Text(!written ?
                res::GetString(L"TransportChangeFailed") :
                res::FormatString(enable ? L"TransportEnabledFormat" : L"TransportDisabledFormat", name));

            RequestServiceRefreshAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to switch a transport on or off.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRestartServiceForTransportsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            TransportsRestartServiceButton().IsEnabled(false);

            auto const enableButton = wil::scope_exit([this]() noexcept
                {
                    try
                    {
                        if (!m_closing)
                        {
                            TransportsRestartServiceButton().IsEnabled(true);
                        }
                    }
                    catch (...)
                    {
                    }
                });

            co_await OfferServiceRestartAsync(
                res::GetString(L"TransportsServiceRestartMessage"),
                TransportsActionText(),
                TransportsProgressRing());

            if (!m_closing)
            {
                RequestServiceRefreshAsync();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to restart the service for the transport changes.")
    }
}
