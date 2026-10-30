// PROTOTYPE. Stand-in for the future crash helper UI (the MIDI Troubleshooting app). See ..\README.md.
//
// WerHelperModule.dll starts this from inside WerFault.exe. It has no window. It logs:
//   - how long after the WER callback started it got to run
//   - when the crashed process and WerFault.exe exit
//   - when the LocalDumps file appears, and when it can be opened with writers denied (complete)
//   - that it is still alive 5 seconds after WerFault.exe is gone

#include <windows.h>
#include <shellapi.h>

#include "protolog.h"

namespace
{
    struct Args
    {
        DWORD Pid{ 0 };
        DWORD WerFaultPid{ 0 };
        DWORD Code{ 0 };
        ULONGLONG CallbackStart{ 0 };
        wchar_t Exe[MAX_PATH]{};
    };

    bool ParseArgs(_Out_ Args& args) noexcept
    {
        args = Args{};

        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
        {
            return false;
        }

        for (int i = 1; i + 1 < argc; i += 2)
        {
            const wchar_t* name = argv[i];
            const wchar_t* value = argv[i + 1];

            if (wcscmp(name, L"--pid") == 0) { args.Pid = wcstoul(value, nullptr, 10); }
            else if (wcscmp(name, L"--werfault-pid") == 0) { args.WerFaultPid = wcstoul(value, nullptr, 10); }
            else if (wcscmp(name, L"--code") == 0) { args.Code = wcstoul(value, nullptr, 16); }
            else if (wcscmp(name, L"--callback-start") == 0) { args.CallbackStart = wcstoull(value, nullptr, 10); }
            else if (wcscmp(name, L"--exe") == 0) { wcscpy_s(args.Exe, value); }
        }

        LocalFree(argv);
        return args.Pid != 0 && args.Exe[0] != L'\0';
    }

    bool ReadDumpFolder(_In_ HKEY root, _In_z_ const wchar_t* key, _Out_writes_(cch) wchar_t* folder, _In_ size_t cch) noexcept
    {
        wchar_t raw[MAX_PATH]{};
        DWORD size = sizeof(raw);
        if (RegGetValueW(root, key, L"DumpFolder", RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, raw, &size) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD len = ExpandEnvironmentStringsW(raw, folder, static_cast<DWORD>(cch));
        return len != 0 && len <= cch;
    }

    // The same order WER uses: the per-exe key, then the LocalDumps defaults, then the built-in
    // default of %LOCALAPPDATA%\CrashDumps.
    void FindDumpPath(_In_ const Args& args, _Out_writes_(cch) wchar_t* path, _In_ size_t cch, _Out_ const wchar_t** source) noexcept
    {
        const wchar_t* base = L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps";
        wchar_t perExe[MAX_PATH]{};
        swprintf_s(perExe, L"%s\\%s", base, args.Exe);

        wchar_t folder[MAX_PATH]{};
        *source = L"default";

        if (ReadDumpFolder(HKEY_LOCAL_MACHINE, perExe, folder, _countof(folder))) { *source = L"hklm_per_exe"; }
        else if (ReadDumpFolder(HKEY_CURRENT_USER, perExe, folder, _countof(folder))) { *source = L"hkcu_per_exe"; }
        else if (ReadDumpFolder(HKEY_LOCAL_MACHINE, base, folder, _countof(folder))) { *source = L"hklm_global"; }
        else if (ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\CrashDumps", folder, _countof(folder)) == 0) { folder[0] = L'\0'; }

        size_t len = wcslen(folder);
        if (len > 0 && folder[len - 1] == L'\\')
        {
            folder[len - 1] = L'\0';
        }

        swprintf_s(path, cch, L"%s\\%s.%lu.dmp", folder, args.Exe, args.Pid);
    }

    double MsSince(_In_ ULONGLONG start, _In_ ULONGLONG now) noexcept
    {
        return (start == 0) ? -1.0 : static_cast<double>(static_cast<LONGLONG>(now - start)) / 10000.0;
    }
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    ULONGLONG started = ProtoNow();

    Args args{};
    bool parsed = ParseArgs(args);

    wchar_t logFolder[MAX_PATH]{};
    if (!ProtoLogFolder(nullptr, logFolder, _countof(logFolder)))
    {
        return 1;
    }

    wchar_t logPath[MAX_PATH]{};
    swprintf_s(logPath, L"%shelper-%lu.log", logFolder, args.Pid);

    if (!parsed)
    {
        ProtoAppendLine(logPath, L"event=bad_args cmdline=\"%s\"", GetCommandLineW());
        return 2;
    }

    BOOL inJob = FALSE;
    IsProcessInJob(GetCurrentProcess(), nullptr, &inJob);

    wchar_t dumpPath[MAX_PATH]{};
    const wchar_t* dumpSource = L"";
    FindDumpPath(args, dumpPath, _countof(dumpPath), &dumpSource);

    ProtoAppendLine(logPath, L"event=helper_start crashed_pid=%lu exe=\"%s\" code=0x%08lX werfault_pid=%lu in_job=%d ms_after_callback_start=%.1f dump_path=\"%s\" dump_folder_from=%s",
        args.Pid, args.Exe, args.Code, args.WerFaultPid, inJob ? 1 : 0, MsSince(args.CallbackStart, started), dumpPath, dumpSource);

    HANDLE crashed = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, args.Pid);
    HANDLE werFault = (args.WerFaultPid != 0) ? OpenProcess(SYNCHRONIZE, FALSE, args.WerFaultPid) : nullptr;
    ProtoAppendLine(logPath, L"event=handles crashed_open=%d werfault_open=%d", crashed != nullptr ? 1 : 0, werFault != nullptr ? 1 : 0);

    bool crashedExited = (crashed == nullptr);
    bool werFaultExited = (werFault == nullptr);
    bool dumpSeen = false;
    bool dumpLockedSeen = false;
    bool dumpComplete = false;
    LONGLONG lastUnlockedSize = -1;
    ULONGLONG deadline = started + 60ULL * 10000000ULL;

    for (;;)
    {
        ULONGLONG now = ProtoNow();

        if (!crashedExited && WaitForSingleObject(crashed, 0) == WAIT_OBJECT_0)
        {
            crashedExited = true;
            DWORD exitCode = 0;
            GetExitCodeProcess(crashed, &exitCode);
            ProtoAppendLine(logPath, L"event=crashed_exited exit_code=0x%08lX ms_after_callback_start=%.1f", exitCode, MsSince(args.CallbackStart, now));
        }

        if (!werFaultExited && WaitForSingleObject(werFault, 0) == WAIT_OBJECT_0)
        {
            werFaultExited = true;
            ProtoAppendLine(logPath, L"event=werfault_exited ms_after_callback_start=%.1f", MsSince(args.CallbackStart, now));
        }

        if (!dumpComplete)
        {
            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if (GetFileAttributesExW(dumpPath, GetFileExInfoStandard, &attributes))
            {
                LONGLONG size = (static_cast<LONGLONG>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
                if (!dumpSeen)
                {
                    dumpSeen = true;
                    ProtoAppendLine(logPath, L"event=dump_seen size=%lld ms_after_callback_start=%.1f crashed_exited=%d", size, MsSince(args.CallbackStart, now), crashedExited ? 1 : 0);
                }

                // Deny writers: this open fails while WER still has the file open for writing.
                HANDLE file = CreateFileW(dumpPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    DWORD error = GetLastError();
                    if (!dumpLockedSeen)
                    {
                        dumpLockedSeen = true;
                        ProtoAppendLine(logPath, L"event=dump_locked error=%lu size=%lld ms_after_callback_start=%.1f", error, size, MsSince(args.CallbackStart, now));
                    }

                    lastUnlockedSize = -1;
                }
                else
                {
                    char signature[4]{};
                    DWORD read = 0;
                    ReadFile(file, signature, sizeof(signature), &read, nullptr);
                    LARGE_INTEGER openSize{};
                    GetFileSizeEx(file, &openSize);
                    CloseHandle(file);

                    bool header = (read == 4 && signature[0] == 'M' && signature[1] == 'D' && signature[2] == 'M' && signature[3] == 'P');

                    // Two unlocked looks in a row at the same size, with the crashed process gone.
                    if (crashedExited && openSize.QuadPart > 0 && openSize.QuadPart == lastUnlockedSize)
                    {
                        dumpComplete = true;
                        ProtoAppendLine(logPath, L"event=dump_complete size=%lld mdmp_header=%d locked_seen=%d ms_after_callback_start=%.1f werfault_exited=%d",
                            openSize.QuadPart, header ? 1 : 0, dumpLockedSeen ? 1 : 0, MsSince(args.CallbackStart, now), werFaultExited ? 1 : 0);
                    }

                    lastUnlockedSize = openSize.QuadPart;
                }
            }
        }

        if (dumpComplete && werFaultExited && crashedExited)
        {
            break;
        }

        if (now > deadline)
        {
            ProtoAppendLine(logPath, L"event=timeout dump_seen=%d dump_complete=%d crashed_exited=%d werfault_exited=%d",
                dumpSeen ? 1 : 0, dumpComplete ? 1 : 0, crashedExited ? 1 : 0, werFaultExited ? 1 : 0);
            break;
        }

        Sleep(50);
    }

    if (crashed != nullptr) { CloseHandle(crashed); }
    if (werFault != nullptr) { CloseHandle(werFault); }

    Sleep(5000);
    ProtoAppendLine(logPath, L"event=helper_alive_5s_after ms_after_callback_start=%.1f", MsSince(args.CallbackStart, ProtoNow()));
    return 0;
}
