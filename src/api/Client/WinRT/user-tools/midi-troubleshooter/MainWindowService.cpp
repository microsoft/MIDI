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

                auto const fileTime = winrt::clock::to_file_time(value);

                FILETIME raw{};
                raw.dwLowDateTime = static_cast<DWORD>(fileTime.value & 0xFFFFFFFF);
                raw.dwHighDateTime = static_cast<DWORD>(fileTime.value >> 32);

                FILETIME local{};

                if (!::FileTimeToLocalFileTime(&raw, &local))
                {
                    return {};
                }

                SYSTEMTIME systemTime{};

                if (!::FileTimeToSystemTime(&local, &systemTime))
                {
                    return {};
                }

                return winrt::hstring{ std::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                    systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                    systemTime.wHour, systemTime.wMinute, systemTime.wSecond) };
            }
            catch (...)
            {
                return {};
            }
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
                        deviceId,
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
            struct Row
            {
                winrt::hstring Key{};
                winrt::hstring Name{};
                winrt::hstring Code{};
                winrt::hstring Description{};
                winrt::hstring Detail{};
                winrt::hstring Module{};
                winrt::hstring Status{};
                TransportItem::Severity Severity{ TransportItem::Severity::Ok };
            };

            std::vector<Row> rows{};

            // Everything the service actually loaded.
            if (snapshot.LoadedTransports != nullptr)
            {
                for (auto const& transport : snapshot.LoadedTransports)
                {
                    Row row{};

                    row.Key = NormalizedGuid(GuidText(transport.TransportId()));
                    row.Name = transport.Name();
                    row.Code = transport.TransportCode();
                    row.Description = transport.Description();
                    row.Status = res::GetString(L"TransportStatusLoaded");
                    row.Severity = TransportItem::Severity::Ok;

                    row.Detail = res::FormatString(L"TransportDetailFormat",
                        transport.Version().empty() ? res::GetString(L"ValueUnknown") : transport.Version(),
                        transport.Author().empty() ? res::GetString(L"ValueUnknown") : transport.Author());

                    rows.push_back(row);
                }
            }

            // Then the registry, so a transport that is registered but did not load is still
            // visible. That is the whole point of this page.
            for (auto const& registered : snapshot.RegisteredTransports)
            {
                auto const key = NormalizedGuid(winrt::hstring{ registered.ClassId });

                auto existing = std::find_if(rows.begin(), rows.end(),
                    [&key](Row const& row) { return !key.empty() && row.Key == key; });

                if (existing != rows.end())
                {
                    if (!registered.ModulePath.empty())
                    {
                        existing->Module = res::FormatString(
                            L"TransportModuleFormat", winrt::hstring{ registered.ModulePath });
                    }

                    if (!registered.Enabled)
                    {
                        existing->Status = res::GetString(L"TransportStatusDisabled");
                        existing->Severity = TransportItem::Severity::Warning;
                    }

                    continue;
                }

                Row row{};

                row.Key = key.empty() ? winrt::hstring{ registered.Name } : key;
                row.Name = winrt::hstring{ registered.Name };

                if (!registered.ModulePath.empty())
                {
                    row.Module = res::FormatString(
                        L"TransportModuleFormat", winrt::hstring{ registered.ModulePath });
                }

                if (!registered.Enabled)
                {
                    row.Status = res::GetString(L"TransportStatusDisabled");
                    row.Severity = TransportItem::Severity::Warning;
                }
                else if (!registered.ModuleRegistered)
                {
                    row.Status = res::GetString(L"TransportStatusNotRegistered");
                    row.Severity = TransportItem::Severity::Error;
                }
                else if (!registered.ModuleFileFound)
                {
                    row.Status = res::GetString(L"TransportStatusFileMissing");
                    row.Severity = TransportItem::Severity::Error;
                }
                else if (registered.BuiltIn)
                {
                    // The service hosts these itself and never reports them as plugins, so
                    // "did not load" would be wrong. A present registration is all there is
                    // to check.
                    row.Status = res::GetString(L"TransportStatusClientComponent");
                    row.Description = res::GetString(L"TransportClientComponentDescription");
                    row.Severity = TransportItem::Severity::Ok;
                }
                else if (snapshot.Service.State != native::ServiceState::Running)
                {
                    row.Status = res::GetString(L"TransportStatusServiceNotRunning");
                    row.Severity = TransportItem::Severity::Warning;
                }
                else
                {
                    row.Status = res::GetString(L"TransportStatusNotLoaded");
                    row.Severity = TransportItem::Severity::Error;
                }

                row.Detail = registered.ModuleVersion.empty() ?
                    winrt::hstring{} :
                    res::FormatString(L"TransportVersionOnlyFormat", winrt::hstring{ registered.ModuleVersion });

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
                auto const& row = rows[i];

                winrt::get_self<TransportItem>(m_transports.GetAt(i))->Update(
                    row.Key, row.Name, row.Code, row.Description,
                    row.Detail, row.Module, row.Status, row.Severity);
            }

            auto notLoaded = uint32_t{ 0 };

            for (auto const& row : rows)
            {
                if (row.Severity != TransportItem::Severity::Ok)
                {
                    notLoaded++;
                }
            }

            TransportsStatusText().Text(notLoaded == 0 ?
                res::FormatString(L"TransportsAllLoadedFormat", static_cast<uint32_t>(rows.size())) :
                res::FormatString(L"TransportsProblemFormat", static_cast<uint32_t>(rows.size()), notLoaded));
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

            native::ServiceOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result]()
                {
                    result = native::RestartMidiService();
                });

            m_busy = false;

            if (m_closing)
            {
                co_return;
            }

            ServiceProgressRing().IsActive(false);

            ServiceStatusText().Text(result.Succeeded ?
                res::GetString(L"ServiceRestarted") :
                res::FormatString(L"ServiceRestartFailedFormat", winrt::hstring{ result.ErrorMessage }));

            RequestServiceRefreshAsync();
        }
        catch (...)
        {
            m_busy = false;

            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to restart the service.");
        }
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
}
