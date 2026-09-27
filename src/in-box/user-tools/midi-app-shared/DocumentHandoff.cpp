// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DocumentHandoff.h"
#include "SingleInstance.h"

namespace midiapp
{
    namespace
    {
        // Enough for a long multiple selection, and finite so a hostile sender cannot make the
        // receiver allocate without bound.
        constexpr size_t MaximumCopyDataBytes = 256 * 1024;
        constexpr size_t MaximumPaths = 64;
    }

    _Use_decl_annotations_
    bool SendDocumentsToExistingInstance(
        std::wstring const& appKey,
        std::vector<std::wstring> const& paths) noexcept
    {
        try
        {
            std::wstring buffer{};

            for (auto const& path : paths)
            {
                if (!path.empty())
                {
                    buffer.append(path);
                    buffer.push_back(L'\0');
                }
            }

            auto const bytes = buffer.size() * sizeof(wchar_t);

            if (buffer.empty() || bytes > MaximumCopyDataBytes)
            {
                return false;
            }

            auto const window = SingleInstance::FindExistingWindow(appKey);

            if (window == nullptr)
            {
                return false;
            }

            // This copy was started by the customer and may raise a window; the running one was
            // not, and would open the document behind whatever is in front.
            DWORD processId{ 0 };

            if (::GetWindowThreadProcessId(window, &processId) != 0 && processId != 0)
            {
                ::AllowSetForegroundWindow(processId);
            }

            COPYDATASTRUCT data{};

            data.dwData = OpenDocumentsMessageId;
            data.cbData = static_cast<DWORD>(bytes);
            data.lpData = buffer.data();

            // Sent rather than posted, because the buffer has to live until the other process
            // has copied it. A window that has stopped answering is given up on.
            DWORD_PTR result{ 0 };

            return ::SendMessageTimeoutW(
                window,
                WM_COPYDATA,
                0,
                reinterpret_cast<LPARAM>(&data),
                SMTO_ABORTIFHUNG | SMTO_BLOCK,
                10000,
                &result) != 0;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> ReadDocumentsFromCopyData(COPYDATASTRUCT const* data) noexcept
    {
        std::vector<std::wstring> paths{};

        try
        {
            if (data == nullptr ||
                data->dwData != OpenDocumentsMessageId ||
                data->lpData == nullptr ||
                data->cbData == 0 ||
                data->cbData > MaximumCopyDataBytes ||
                (data->cbData % sizeof(wchar_t)) != 0)
            {
                return paths;
            }

            auto const* const characters = static_cast<wchar_t const*>(data->lpData);
            auto const count = data->cbData / sizeof(wchar_t);

            // Walked by length rather than trusting the other process to have ended it.
            size_t start = 0;

            for (size_t index = 0; index < count && paths.size() < MaximumPaths; ++index)
            {
                if (characters[index] != L'\0')
                {
                    continue;
                }

                if (index > start)
                {
                    paths.emplace_back(characters + start, index - start);
                }

                start = index + 1;
            }

            if (start < count && paths.size() < MaximumPaths)
            {
                paths.emplace_back(characters + start, count - start);
            }
        }
        catch (...)
        {
            paths.clear();
        }

        return paths;
    }
}
