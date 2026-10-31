// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ServiceControl.h"

// The crash dumps Windows Error Reporting (WER) saves for the Windows MIDI Services tools, and the
// MIDI service dumps saved alongside them.
//
// The Tools installer adds a LocalDumps entry for each tool, so WER writes its dumps to
// %LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\<tool>, keeping the newest 10. That folder belongs to the
// customer, so they can delete the dumps, and WER can replace the oldest. MIDI service dumps go in
// the midisrv folder beside them. Everything here blocks, so callers run it on a background thread.
namespace miditroubleshooter::crashdumps
{
    // The service dumps this page saves are kept to the same number WER keeps for each tool.
    inline constexpr size_t MaximumServiceDumps = 10;

    inline constexpr wchar_t ServiceDumpFolderName[] = L"midisrv";

    struct DumpFile
    {
        std::wstring Path{};
        std::wstring FileName{};

        // The folder the dump is in, which is the tool's exe name without ".exe", or midisrv.
        std::wstring FolderName{};

        uint64_t Size{ 0 };

        // FILETIME of the last write, which for a dump is when it was saved
        uint64_t WriteTime{ 0 };

        bool IsServiceDump{ false };
    };

    // %LOCALAPPDATA%\Microsoft\MIDI\CrashDumps. Empty if Windows can't say where that is.
    std::wstring RootFolder() noexcept;

    // Where WER saves this tool's dumps: the DumpFolder the installer wrote for it, or the
    // folder it would have written when there's no entry.
    std::wstring ToolDumpFolder(_In_ std::wstring const& executableName) noexcept;

    // Whether WER has a LocalDumps entry for this exe at all. Without one it saves no dump.
    bool HasLocalDumpsEntry(_In_ std::wstring const& executableName) noexcept;

    // Every dump under the root folder, newest first.
    std::vector<DumpFile> ListDumps() noexcept;

    enum class CrashDumpWait
    {
        Saved,

        // WER finished, or ran out of time, and there's no dump.
        NotSaved,

        // the caller gave up
        Canceled
    };

    struct CrashDumpWaitResult
    {
        CrashDumpWait Outcome{ CrashDumpWait::NotSaved };
        std::wstring Path{};
    };

    // Waits for WerFault.exe to finish writing the crashed tool's dump, which it holds open until
    // it exits, and for the file to be readable. Both process ids came from another process, so
    // the WerFault id is only waited on if it really is WerFault.exe.
    CrashDumpWaitResult WaitForCrashDump(
        _In_ DWORD crashedProcessId,
        _In_ std::wstring const& executableName,
        _In_ DWORD werFaultProcessId,
        _In_ std::atomic<bool> const& cancel) noexcept;

    struct ServiceDumpResult
    {
        ServiceOperationResult Operation{};
        std::wstring Path{};
    };

    // A memory dump of the MIDI service into the midisrv folder, named for the tool that crashed
    // when there is one so the two pair up, then the oldest beyond MaximumServiceDumps are
    // deleted. Needs administrator rights.
    ServiceDumpResult SaveServiceDump(
        _In_ std::wstring const& crashedExecutable,
        _In_ DWORD crashedProcessId) noexcept;

    // Only files under the root folder are ever deleted, whatever path is passed in.
    bool DeleteDump(_In_ std::wstring const& path) noexcept;

    // One zip holding the dumps, each under its folder's name. The caller has already chosen
    // where it goes.
    bool ZipDumps(_In_ std::vector<DumpFile> const& dumps, _In_ std::wstring const& zipPath, _Out_ std::wstring& errorMessage) noexcept;

    bool OpenFolderInExplorer(_In_ std::wstring const& folder) noexcept;

    // Opens Explorer on the folder with the file selected.
    bool ShowFileInExplorer(_In_ std::wstring const& path) noexcept;
}
