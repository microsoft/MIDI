// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ProcessRunner.h"

namespace miditroubleshooter
{
    namespace
    {
        std::wstring FormatSystemError(_In_ DWORD const error) noexcept
        {
            try
            {
                wil::unique_hlocal_string message;

                auto const length = ::FormatMessageW(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr,
                    error,
                    0,
                    reinterpret_cast<LPWSTR>(message.put()),
                    0,
                    nullptr);

                if (length > 0 && message)
                {
                    std::wstring text{ message.get() };

                    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n'))
                    {
                        text.pop_back();
                    }

                    return text;
                }
            }
            catch (...)
            {
            }

            return std::format(L"Error 0x{:08X}", error);
        }

        // The console tools switch stdout to UTF-8 when they detect redirection, so that is
        // the only encoding this has to handle.
        std::wstring Utf8ToWide(_In_ std::string const& value) noexcept
        {
            try
            {
                if (value.empty())
                {
                    return {};
                }

                auto const required = ::MultiByteToWideChar(
                    CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

                if (required <= 0)
                {
                    return {};
                }

                std::wstring converted(static_cast<size_t>(required), L'\0');

                ::MultiByteToWideChar(
                    CP_UTF8, 0, value.data(), static_cast<int>(value.size()), converted.data(), required);

                return converted;
            }
            catch (...)
            {
                return {};
            }
        }

        // A quoted, escaped copy of the whole command line. CreateProcess writes to its
        // lpCommandLine buffer, so it can never be a literal.
        std::wstring BuildCommandLine(
            _In_ std::wstring const& executablePath,
            _In_ std::wstring const& arguments) noexcept
        {
            std::wstring commandLine{ L"\"" };
            commandLine += executablePath;
            commandLine += L"\"";

            if (!arguments.empty())
            {
                commandLine += L" ";
                commandLine += arguments;
            }

            return commandLine;
        }

        ProcessResult Run(
            _In_ std::wstring const& executablePath,
            _In_ std::wstring const& arguments,
            _In_ std::chrono::seconds const timeout,
            _In_ bool const captureOutput) noexcept
        {
            ProcessResult result{};

            try
            {
                if (executablePath.empty())
                {
                    result.ErrorMessage = L"No path was supplied for the program to run.";
                    return result;
                }

                SECURITY_ATTRIBUTES securityAttributes{};
                securityAttributes.nLength = sizeof(securityAttributes);
                securityAttributes.bInheritHandle = TRUE;

                wil::unique_handle readPipe;
                wil::unique_handle writePipe;

                if (captureOutput)
                {
                    if (!::CreatePipe(readPipe.put(), writePipe.put(), &securityAttributes, 0))
                    {
                        result.ErrorMessage = FormatSystemError(::GetLastError());
                        return result;
                    }

                    // only the write end goes to the child, or the read never sees end of file
                    if (!::SetHandleInformation(readPipe.get(), HANDLE_FLAG_INHERIT, 0))
                    {
                        result.ErrorMessage = FormatSystemError(::GetLastError());
                        return result;
                    }
                }

                STARTUPINFOW startupInfo{};
                startupInfo.cb = sizeof(startupInfo);
                startupInfo.dwFlags = STARTF_USESHOWWINDOW;
                startupInfo.wShowWindow = SW_HIDE;

                if (captureOutput)
                {
                    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
                    startupInfo.hStdOutput = writePipe.get();
                    startupInfo.hStdError = writePipe.get();
                    startupInfo.hStdInput = nullptr;
                }

                PROCESS_INFORMATION processInformation{};

                auto commandLine = BuildCommandLine(executablePath, arguments);

                auto const created = ::CreateProcessW(
                    executablePath.c_str(),
                    commandLine.data(),
                    nullptr,
                    nullptr,
                    captureOutput ? TRUE : FALSE,
                    CREATE_NO_WINDOW,
                    nullptr,
                    nullptr,
                    &startupInfo,
                    &processInformation);

                if (!created)
                {
                    result.ErrorMessage = FormatSystemError(::GetLastError());
                    return result;
                }

                wil::unique_handle processHandle{ processInformation.hProcess };
                wil::unique_handle threadHandle{ processInformation.hThread };

                result.Started = true;

                // released here so the read below reaches end of file when the child exits
                writePipe.reset();

                std::string rawOutput{};

                if (captureOutput)
                {
                    std::array<char, 8192> buffer{};

                    for (;;)
                    {
                        DWORD bytesRead{ 0 };

                        if (!::ReadFile(readPipe.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
                        {
                            break;
                        }

                        if (bytesRead == 0)
                        {
                            break;
                        }

                        rawOutput.append(buffer.data(), bytesRead);
                    }
                }

                auto const waitResult = ::WaitForSingleObject(
                    processHandle.get(),
                    static_cast<DWORD>(std::chrono::milliseconds{ timeout }.count()));

                if (waitResult == WAIT_TIMEOUT)
                {
                    result.TimedOut = true;

                    // A tool that will not finish is worse than no output at all, and leaving
                    // it running would hold the pipe and the trace session open.
                    ::TerminateProcess(processHandle.get(), 1);
                    ::WaitForSingleObject(processHandle.get(), 5000);
                }

                DWORD exitCode{ 0 };

                if (::GetExitCodeProcess(processHandle.get(), &exitCode))
                {
                    result.ExitCode = exitCode;
                }

                result.Output = Utf8ToWide(rawOutput);
            }
            catch (winrt::hresult_error const& ex)
            {
                result.ErrorMessage = ex.message();
                MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to run an external program.");
            }
            catch (...)
            {
                result.ErrorMessage = L"An unexpected error occurred while running the program.";
                MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to run an external program.");
            }

            return result;
        }
    }

    _Use_decl_annotations_
    ProcessResult RunCapture(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds timeout) noexcept
    {
        return Run(executablePath, arguments, timeout, true);
    }

    _Use_decl_annotations_
    ProcessResult RunToCompletion(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds timeout) noexcept
    {
        return Run(executablePath, arguments, timeout, false);
    }
}
