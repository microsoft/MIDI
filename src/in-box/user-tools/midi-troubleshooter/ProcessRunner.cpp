// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ProcessRunner.h"
#include "StringResources.h"

namespace miditroubleshooter
{
    namespace
    {
        // Short enough that a child which stops producing is noticed quickly, long enough that
        // a chatty tool is not polled needlessly.
        constexpr DWORD ReadPollIntervalMilliseconds = 50;

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

            return std::wstring{ resources::FormatString(L"SystemErrorCodeFormat", error) };
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

        // The longest prefix ending on a whole UTF-8 character, so a character split across two reads is decoded once.
        size_t CompleteUtf8Length(_In_ std::string_view const bytes) noexcept
        {
            auto const size = bytes.size();

            for (size_t back = 1; back <= 4 && back <= size; back++)
            {
                auto const byte = static_cast<unsigned char>(bytes[size - back]);

                if ((byte & 0xC0) == 0x80)
                {
                    continue;
                }

                size_t const length =
                    (byte & 0xE0) == 0xC0 ? 2 :
                    (byte & 0xF0) == 0xE0 ? 3 :
                    (byte & 0xF8) == 0xF0 ? 4 : 1;

                return length > back ? size - back : size;
            }

            return size;
        }

        // False when the handler failed, so the caller stops handing output on.
        bool DeliverOutput(
            _In_ std::string const& rawOutput,
            _Inout_ size_t& delivered,
            _In_ OutputReceivedHandler const& onOutputReceived) noexcept
        {
            try
            {
                auto const pending = std::string_view{ rawOutput }.substr(delivered);
                auto const complete = CompleteUtf8Length(pending);

                if (complete > 0)
                {
                    auto const text = Utf8ToWide(std::string{ pending.substr(0, complete) });

                    delivered += complete;

                    onOutputReceived(text);
                }

                return true;
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to pass on output as it arrived.")

            return false;
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

        // maximumRawBytes is 0 for text output. Otherwise the output is the contents of a file:
        // it's kept as bytes, standard error is thrown away so that it can't mix in, and the
        // program is stopped as soon as it has written more than that many bytes. An empty
        // working folder means this process's own.
        ProcessResult Run(
            _In_ std::wstring const& executablePath,
            _In_ std::wstring const& arguments,
            _In_ std::wstring const& workingFolder,
            _In_ std::chrono::seconds const timeout,
            _In_ bool const captureOutput,
            _In_ OutputReceivedHandler const& onOutputReceived,
            _In_ size_t const maximumRawBytes) noexcept
        {
            ProcessResult result{};

            try
            {
                if (executablePath.empty())
                {
                    result.ErrorMessage = resources::GetString(L"ProcessErrorNoPath");
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

                auto const rawOutputWanted = captureOutput && maximumRawBytes > 0;

                wil::unique_hfile discardedErrors;

                if (rawOutputWanted)
                {
                    discardedErrors.reset(::CreateFileW(
                        L"NUL",
                        GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        &securityAttributes,
                        OPEN_EXISTING,
                        0,
                        nullptr));

                    if (!discardedErrors)
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
                    startupInfo.hStdError = rawOutputWanted ? discardedErrors.get() : writePipe.get();
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
                    workingFolder.empty() ? nullptr : workingFolder.c_str(),
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

                auto const deadline = std::chrono::steady_clock::now() + timeout;

                std::string rawOutput{};

                if (captureOutput)
                {
                    std::array<char, 8192> buffer{};

                    size_t delivered{ 0 };
                    bool streaming{ static_cast<bool>(onOutputReceived) };

                    bool exited{ false };

                    // An anonymous pipe cannot be read with an overlapped handle, and a blocking
                    // ReadFile on a child that never exits would never return, so the timeout
                    // below would never be reached. Peeking first keeps every read short.
                    for (;;)
                    {
                        // Checked before the read rather than after it. A tracer or a merge step
                        // writes progress continuously, so a deadline tested only on an empty
                        // pipe is never reached and the caller waits forever.
                        if (std::chrono::steady_clock::now() >= deadline)
                        {
                            result.TimedOut = true;
                            break;
                        }

                        DWORD available{ 0 };

                        if (!::PeekNamedPipe(readPipe.get(), nullptr, 0, nullptr, &available, nullptr))
                        {
                            break;
                        }

                        if (available > 0)
                        {
                            DWORD bytesRead{ 0 };

                            auto const toRead = std::min(available, static_cast<DWORD>(buffer.size()));

                            if (!::ReadFile(readPipe.get(), buffer.data(), toRead, &bytesRead, nullptr) ||
                                bytesRead == 0)
                            {
                                break;
                            }

                            rawOutput.append(buffer.data(), bytesRead);

                            if (rawOutputWanted && rawOutput.size() > maximumRawBytes)
                            {
                                result.OutputLimitReached = true;
                                break;
                            }

                            if (streaming)
                            {
                                streaming = DeliverOutput(rawOutput, delivered, onOutputReceived);
                            }

                            continue;
                        }

                        // Nothing buffered. The child having exited is not enough on its own,
                        // because output written just before it exited is still in the pipe, so
                        // the pipe is read dry once more after the exit before this stops.
                        if (exited)
                        {
                            break;
                        }

                        exited = ::WaitForSingleObject(processHandle.get(), ReadPollIntervalMilliseconds) == WAIT_OBJECT_0;
                    }
                }

                if (!result.TimedOut && !result.OutputLimitReached)
                {
                    auto const remaining = deadline - std::chrono::steady_clock::now();

                    auto const remainingMilliseconds = remaining > std::chrono::steady_clock::duration::zero() ?
                        std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count() : 0;

                    if (::WaitForSingleObject(processHandle.get(), static_cast<DWORD>(remainingMilliseconds)) == WAIT_TIMEOUT)
                    {
                        result.TimedOut = true;
                    }
                }

                if (result.TimedOut || result.OutputLimitReached)
                {
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

                if (rawOutputWanted)
                {
                    result.RawOutput = std::move(rawOutput);
                }
                else
                {
                    result.Output = Utf8ToWide(rawOutput);
                }
            }
            catch (winrt::hresult_error const& ex)
            {
                result.ErrorMessage = ex.message();
                MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to run an external program.");
            }
            catch (...)
            {
                result.ErrorMessage = resources::GetString(L"ProcessErrorRunUnexpected");
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
        return Run(executablePath, arguments, {}, timeout, true, {}, 0);
    }

    _Use_decl_annotations_
    ProcessResult RunCapture(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds timeout,
        OutputReceivedHandler const& onOutputReceived) noexcept
    {
        return Run(executablePath, arguments, {}, timeout, true, onOutputReceived, 0);
    }

    _Use_decl_annotations_
    ProcessResult RunCaptureIn(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::wstring const& workingFolder,
        std::chrono::seconds timeout) noexcept
    {
        return Run(executablePath, arguments, workingFolder, timeout, true, {}, 0);
    }

    _Use_decl_annotations_
    ProcessResult RunCaptureBytes(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::wstring const& workingFolder,
        std::chrono::seconds timeout,
        size_t maximumBytes) noexcept
    {
        return Run(executablePath, arguments, workingFolder, timeout, true, {}, std::max<size_t>(maximumBytes, 1));
    }

    _Use_decl_annotations_
    ProcessResult RunToCompletion(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds timeout) noexcept
    {
        return Run(executablePath, arguments, {}, timeout, false, {}, 0);
    }

    _Use_decl_annotations_
    ProcessResult StartAndWatch(
        std::wstring const& executablePath,
        std::wstring const& arguments,
        std::chrono::seconds settleTime) noexcept
    {
        ProcessResult result{};

        try
        {
            if (executablePath.empty())
            {
                result.ErrorMessage = resources::GetString(L"ProcessErrorNoPath");
                return result;
            }

            STARTUPINFOW startupInfo{};
            startupInfo.cb = sizeof(startupInfo);
            startupInfo.dwFlags = STARTF_USESHOWWINDOW;
            startupInfo.wShowWindow = SW_HIDE;

            PROCESS_INFORMATION processInformation{};

            auto commandLine = BuildCommandLine(executablePath, arguments);

            auto const created = ::CreateProcessW(
                executablePath.c_str(),
                commandLine.data(),
                nullptr,
                nullptr,
                FALSE,
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

            auto const milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(settleTime).count();

            if (::WaitForSingleObject(processHandle.get(), static_cast<DWORD>(milliseconds)) == WAIT_TIMEOUT)
            {
                result.StillRunning = true;
            }
            else
            {
                DWORD exitCode{ 0 };

                if (::GetExitCodeProcess(processHandle.get(), &exitCode))
                {
                    result.ExitCode = exitCode;
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            result.ErrorMessage = ex.message();
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to start an external program.");
        }
        catch (...)
        {
            result.ErrorMessage = resources::GetString(L"ProcessErrorStartUnexpected");
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to start an external program.");
        }

        return result;
    }
}
