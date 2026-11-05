// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CrashDumpsPanel.xaml.h"
#if __has_include("CrashDumpsPanel.g.cpp")
#include "CrashDumpsPanel.g.cpp"
#endif

#include "BackgroundWork.h"
#include "StringResources.h"

#include <filesystem>

namespace native = ::miditroubleshooter;
namespace dumps = ::miditroubleshooter::crashdumps;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        winrt::hstring FormatWhen(_In_ uint64_t const fileTime) noexcept
        {
            try
            {
                FILETIME utc{};
                utc.dwLowDateTime = static_cast<DWORD>(fileTime & 0xFFFFFFFF);
                utc.dwHighDateTime = static_cast<DWORD>(fileTime >> 32);

                SYSTEMTIME utcTime{};
                SYSTEMTIME localTime{};

                if (!::FileTimeToSystemTime(&utc, &utcTime) ||
                    !::SystemTimeToTzSpecificLocalTime(nullptr, &utcTime, &localTime))
                {
                    return {};
                }

                wchar_t date[80]{};
                wchar_t time[80]{};

                if (::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &localTime, nullptr, date, ARRAYSIZE(date), nullptr) == 0 ||
                    ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &localTime, nullptr, time, ARRAYSIZE(time)) == 0)
                {
                    return {};
                }

                return res::FormatString(L"CrashDumpsDateTimeFormat", std::wstring{ date }, std::wstring{ time });
            }
            catch (...)
            {
                return {};
            }
        }

        winrt::hstring FormatSize(_In_ uint64_t const size) noexcept
        {
            wchar_t text[64]{};

            if (FAILED(::StrFormatByteSizeEx(size, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, text, ARRAYSIZE(text))))
            {
                return {};
            }

            return winrt::hstring{ text };
        }

        std::wstring ZipTimestamp()
        {
            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            return std::format(L"{:04}{:02}{:02}-{:02}{:02}{:02}",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        }
    }

    CrashDumpsPanel::CrashDumpsPanel()
    {
        InitializeComponent();
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::Initialize(Host host) noexcept
    {
        try
        {
            m_host = std::move(host);

            DumpsListView().ItemsSource(m_items);

            midiapp::MakeLiveStatusRegion(ServiceDumpStatusText());
            midiapp::MakeLiveStatusRegion(ListStatusText());

            m_initialized = true;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set up the crash dumps page.")
    }

    void CrashDumpsPanel::OnShown() noexcept
    {
        if (m_initialized)
        {
            RefreshAsync();
        }
    }

    void CrashDumpsPanel::Shutdown() noexcept
    {
        m_initialized = false;

        if (m_cancelWait)
        {
            m_cancelWait->store(true);
        }
    }

    _Use_decl_annotations_
    winrt::hstring CrashDumpsPanel::ToolDisplayName(std::wstring const& folderOrExecutable) const noexcept
    {
        try
        {
            auto stem = std::filesystem::path{ folderOrExecutable }.stem().wstring();

            if (_wcsicmp(stem.c_str(), dumps::ServiceDumpFolderName) == 0)
            {
                return res::GetString(L"CrashDumpsServiceName");
            }

            std::transform(stem.begin(), stem.end(), stem.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            // The tools this version knows have a name in the resources. A missing key comes
            // back as the key itself.
            auto const key = L"CrashDumpsToolName_" + stem;
            auto const name = res::GetString(key);

            return name == key ? winrt::hstring{ folderOrExecutable } : name;
        }
        catch (...)
        {
            return winrt::hstring{ folderOrExecutable };
        }
    }

    // ------------------------------------------------------------------------------- the crash

    _Use_decl_annotations_
    void CrashDumpsPanel::ShowCrash(::miditroubleshooter::CommandLineOptions const& options) noexcept
    {
        try
        {
            if (!m_initialized || !options.HasCrashReport())
            {
                return;
            }

            // Another crash arriving while the last one is still being waited for replaces it.
            if (m_cancelWait)
            {
                m_cancelWait->store(true);
            }

            m_crashedProcessId = options.CrashedProcessId;
            m_crashedExecutable = options.CrashedExecutable;
            m_werFaultProcessId = options.WerFaultProcessId;
            m_crashDumpPath.clear();
            m_crashState = CrashState::Waiting;

            ShowCrashState();

            WaitForCrashDumpAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a crash.")
    }

    void CrashDumpsPanel::ShowCrashState() noexcept
    {
        try
        {
            auto const bar = CrashBar();

            if (m_crashState == CrashState::None)
            {
                bar.IsOpen(false);
                return;
            }

            auto const toolName = ToolDisplayName(m_crashedExecutable);

            bar.Title(res::FormatString(L"CrashDumpsCrashTitleFormat", toolName));

            auto const waiting = m_crashState == CrashState::Waiting;

            CrashWaitRing().IsActive(waiting);
            CrashWaitRing().Visibility(waiting ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            ShowCrashDumpButton().Visibility(m_crashState == CrashState::Saved ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            switch (m_crashState)
            {
            case CrashState::Waiting:
                bar.Message(res::GetString(L"CrashDumpsCrashWaiting"));
                break;

            case CrashState::Saved:
                bar.Message(res::FormatString(L"CrashDumpsCrashSavedFormat", m_crashDumpPath));
                break;

            default:
                // No LocalDumps entry means the Tools installer didn't set this tool up, which is
                // worth saying differently from WER simply not saving one.
                bar.Message(dumps::HasLocalDumpsEntry(m_crashedExecutable) ?
                    res::GetString(L"CrashDumpsCrashNotSaved") :
                    res::GetString(L"CrashDumpsCrashNotSetUp"));
                break;
            }

            bar.IsOpen(true);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the crash banner.")
    }

    winrt::fire_and_forget CrashDumpsPanel::WaitForCrashDumpAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            auto const cancel = std::make_shared<std::atomic<bool>>(false);
            m_cancelWait = cancel;

            auto const processId = m_crashedProcessId;
            auto const executable = m_crashedExecutable;
            auto const werFault = m_werFaultProcessId;

            dumps::CrashDumpWaitResult result{};

            co_await native::RunOnBackgroundAsync([&result, processId, executable, werFault, cancel]()
                {
                    result = dumps::WaitForCrashDump(processId, executable, werFault, *cancel);
                });

            // a newer crash, or the window closing, took over
            if (cancel->load() || !m_initialized || result.Outcome == dumps::CrashDumpWait::Canceled)
            {
                co_return;
            }

            m_cancelWait.reset();

            m_crashDumpPath = result.Path;
            m_crashState = result.Outcome == dumps::CrashDumpWait::Saved ? CrashState::Saved : CrashState::NotSaved;

            ShowCrashState();
            RefreshAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to wait for a crash dump.")
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::OnShowCrashDumpClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (!m_crashDumpPath.empty())
            {
                dumps::ShowFileInExplorer(m_crashDumpPath);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the crash dump.")
    }

    // ------------------------------------------------------------------------ MIDI service dump

    _Use_decl_annotations_
    winrt::fire_and_forget CrashDumpsPanel::OnSaveServiceDumpClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_serviceDumpBusy)
            {
                co_return;
            }

            if (!m_host.Elevated)
            {
                // Reading another process's memory needs administrator rights. The elevated
                // copy opens here with the same crash and saves the dump as it starts.
                if (!m_host.RelaunchElevatedForServiceDump || !m_host.RelaunchElevatedForServiceDump())
                {
                    ServiceDumpStatusText().Text(res::GetString(L"CrashDumpsServiceDumpDeclined"));
                    ServiceDumpStatusText().Visibility(xaml::Visibility::Visible);
                }

                co_return;
            }

            SaveServiceDumpAsync();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start a MIDI service dump.")
    }

    void CrashDumpsPanel::SaveServiceDumpNow() noexcept
    {
        if (m_initialized && m_host.Elevated)
        {
            SaveServiceDumpAsync();
        }
    }

    winrt::fire_and_forget CrashDumpsPanel::SaveServiceDumpAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_serviceDumpBusy)
            {
                co_return;
            }

            m_serviceDumpBusy = true;

            ServiceDumpButton().IsEnabled(false);
            CrashServiceDumpButton().IsEnabled(false);
            ServiceDumpRing().IsActive(true);
            ServiceDumpRing().Visibility(xaml::Visibility::Visible);
            ServiceDumpStatusText().Text(res::GetString(L"CrashDumpsServiceDumpSaving"));
            ServiceDumpStatusText().Visibility(xaml::Visibility::Visible);

            auto const executable = m_crashedExecutable;
            auto const processId = m_crashedProcessId;

            dumps::ServiceDumpResult result{};

            co_await native::RunOnBackgroundAsync([&result, executable, processId]()
                {
                    result = dumps::SaveServiceDump(executable, processId);
                });

            m_serviceDumpBusy = false;

            if (!m_initialized)
            {
                co_return;
            }

            ServiceDumpRing().IsActive(false);
            ServiceDumpRing().Visibility(xaml::Visibility::Collapsed);
            ServiceDumpButton().IsEnabled(true);
            CrashServiceDumpButton().IsEnabled(true);

            ServiceDumpStatusText().Text(result.Operation.Succeeded ?
                res::FormatString(L"CrashDumpsServiceDumpSavedFormat", result.Path) :
                res::FormatString(L"CrashDumpsServiceDumpFailedFormat", result.Operation.ErrorMessage));

            RefreshAsync();
        }
        catch (...)
        {
            m_serviceDumpBusy = false;
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to save a MIDI service dump.");
        }
    }

    // ------------------------------------------------------------------------------ saved dumps

    winrt::fire_and_forget CrashDumpsPanel::RefreshAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            if (m_listBusy)
            {
                co_return;
            }

            SetListBusy(true);

            std::vector<dumps::DumpFile> found{};

            co_await native::RunOnBackgroundAsync([&found]()
                {
                    found = dumps::ListDumps();
                });

            SetListBusy(false);

            if (!m_initialized)
            {
                co_return;
            }

            m_dumps = std::move(found);
            m_items.Clear();

            for (auto const& dump : m_dumps)
            {
                auto const title = ToolDisplayName(dump.FolderName);
                auto const detail = res::FormatString(L"CrashDumpsItemDetailFormat", FormatWhen(dump.WriteTime), FormatSize(dump.Size));

                auto item = winrt::make_self<CrashDumpItem>();
                item->Initialize(
                    winrt::hstring{ dump.Path },
                    title,
                    detail,
                    winrt::hstring{ dump.FileName },
                    res::FormatString(L"CrashDumpsItemAccessibleFormat", title, detail));

                m_items.Append(*item);
            }

            NoDumpsText().Visibility(m_dumps.empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            DumpsListView().Visibility(m_dumps.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            UpdateListButtons();
        }
        catch (...)
        {
            SetListBusy(false);
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to list the crash dumps.");
        }
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::SetListBusy(bool const busy) noexcept
    {
        try
        {
            m_listBusy = busy;

            ListBusyRing().IsActive(busy);
            ListBusyRing().Visibility(busy ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            RefreshButton().IsEnabled(!busy);

            UpdateListButtons();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the crash dump list as busy.")
    }

    void CrashDumpsPanel::UpdateListButtons() noexcept
    {
        try
        {
            auto const anySelected = DumpsListView().SelectedItems().Size() > 0;

            ZipButton().IsEnabled(anySelected && !m_listBusy);
            DeleteButton().IsEnabled(anySelected && !m_listBusy);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the crash dump buttons.")
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::ShowListStatus(winrt::hstring const& text) noexcept
    {
        try
        {
            ListStatusText().Text(text);
            ListStatusText().Visibility(text.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the crash dump status.")
    }

    std::vector<dumps::DumpFile> CrashDumpsPanel::SelectedDumps()
    {
        std::vector<dumps::DumpFile> selected{};

        for (auto const& item : DumpsListView().SelectedItems())
        {
            auto const row = item.try_as<miditroubleshooter::CrashDumpItem>();

            if (row == nullptr)
            {
                continue;
            }

            auto const path = std::wstring{ row.Path() };

            // Only what this page listed, never a path from anywhere else.
            auto const found = std::find_if(m_dumps.begin(), m_dumps.end(),
                [&path](dumps::DumpFile const& dump) { return dump.Path == path; });

            if (found != m_dumps.end())
            {
                selected.push_back(*found);
            }
        }

        return selected;
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::OnDumpsSelectionChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        UpdateListButtons();
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::OnRefreshClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowListStatus({});
        RefreshAsync();
    }

    _Use_decl_annotations_
    void CrashDumpsPanel::OnOpenFolderClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (!dumps::OpenFolderInExplorer(dumps::RootFolder()))
            {
                ShowListStatus(res::GetString(L"CrashDumpsOpenFolderFailed"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open the crash dump folder.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget CrashDumpsPanel::OnZipClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const selected = SelectedDumps();

            if (selected.empty() || m_listBusy)
            {
                co_return;
            }

            auto const path = ShowSaveZipDialog(std::format(L"midi-crash-dumps-{}.zip", ZipTimestamp()));

            if (path.empty())
            {
                co_return;
            }

            SetListBusy(true);
            ShowListStatus(res::GetString(L"CrashDumpsZipSaving"));

            bool written{ false };
            std::wstring errorMessage{};

            co_await native::RunOnBackgroundAsync([&written, &errorMessage, &selected, &path]()
                {
                    written = dumps::ZipDumps(selected, path, errorMessage);
                });

            SetListBusy(false);

            if (!m_initialized)
            {
                co_return;
            }

            if (written)
            {
                ShowListStatus(res::FormatString(L"CrashDumpsZipSavedFormat", path));
                dumps::ShowFileInExplorer(path);
            }
            else
            {
                ShowListStatus(winrt::hstring{ errorMessage });
            }
        }
        catch (...)
        {
            SetListBusy(false);
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to zip the crash dumps.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget CrashDumpsPanel::OnDeleteClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const selected = SelectedDumps();

            if (selected.empty() || m_listBusy)
            {
                co_return;
            }

            controls::ContentDialog dialog{};
            dialog.XamlRoot(XamlRoot());
            dialog.Title(winrt::box_value(res::GetString(L"CrashDumpsDeleteTitle")));
            dialog.Content(winrt::box_value(res::FormatString(L"CrashDumpsDeleteMessageFormat", selected.size())));
            dialog.PrimaryButtonText(res::GetString(L"CrashDumpsDeleteConfirm"));
            dialog.CloseButtonText(res::GetString(L"CrashDumpsDeleteCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Close);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary || !m_initialized)
            {
                co_return;
            }

            SetListBusy(true);

            size_t failed{ 0 };

            co_await native::RunOnBackgroundAsync([&failed, &selected]()
                {
                    for (auto const& dump : selected)
                    {
                        if (!dumps::DeleteDump(dump.Path))
                        {
                            failed++;
                        }
                    }
                });

            SetListBusy(false);

            if (!m_initialized)
            {
                co_return;
            }

            // A dump WER is still writing, or one an admin copy saved, can refuse.
            ShowListStatus(failed == 0 ? winrt::hstring{} : res::FormatString(L"CrashDumpsDeleteFailedFormat", failed));

            RefreshAsync();
        }
        catch (...)
        {
            SetListBusy(false);
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to delete crash dumps.");
        }
    }

    // ------------------------------------------------------------------------------ the dialog

    HWND CrashDumpsPanel::WindowHandle() const noexcept
    {
        try
        {
            return m_host.WindowHandle ? m_host.WindowHandle() : nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // The Win32 dialog, because the WinRT pickers never finish in an elevated process.
    _Use_decl_annotations_
    std::wstring CrashDumpsPanel::ShowSaveZipDialog(std::wstring const& suggestedFileName) noexcept
    {
        try
        {
            winrt::com_ptr<IFileSaveDialog> dialog{};

            if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            auto const label = res::GetString(L"SaveZipFileType");

            COMDLG_FILTERSPEC const filters[]
            {
                { label.c_str(), L"*.zip" }
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetDefaultExtension(L"zip"));
            LOG_IF_FAILED(dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));
            LOG_IF_FAILED(dialog->SetFileName(suggestedFileName.c_str()));

            // canceling is reported as a failure, so it isn't logged
            if (FAILED(dialog->Show(WindowHandle())))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the save dialog.")

        return {};
    }
}
