// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "InstanceHandoff.h"

namespace midiplayer
{
    namespace
    {
        // Enough for a very long multiple selection, and finite so a hostile sender cannot make
        // the player allocate without bound.
        constexpr size_t MaximumCopyDataBytes = 256 * 1024;
        constexpr size_t MaximumPaths = 512;
    }

    _Use_decl_annotations_
    bool SendFilesToExistingInstance(std::vector<std::wstring> const& paths) noexcept
    {
        try
        {
            if (paths.empty())
            {
                return false;
            }

            auto const window = midiapp::SingleInstance::FindExistingWindow(InstanceKey);

            if (window == nullptr)
            {
                return false;
            }

            std::wstring buffer{};

            for (auto const& path : paths)
            {
                if (path.empty())
                {
                    continue;
                }

                buffer.append(path);
                buffer.push_back(L'\0');
            }

            if (buffer.empty())
            {
                return false;
            }

            auto const bytes = buffer.size() * sizeof(wchar_t);

            if (bytes > MaximumCopyDataBytes)
            {
                return false;
            }

            COPYDATASTRUCT data{};

            data.dwData = OpenFilesMessageId;
            data.cbData = static_cast<DWORD>(bytes);
            data.lpData = const_cast<wchar_t*>(buffer.c_str());

            // SendMessage rather than Post, because the buffer has to stay alive until the other
            // process has copied it.
            ::SendMessageW(window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data));

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> ReadFilesFromCopyData(COPYDATASTRUCT const* const data) noexcept
    {
        std::vector<std::wstring> paths{};

        try
        {
            if (data == nullptr ||
                data->dwData != OpenFilesMessageId ||
                data->lpData == nullptr ||
                data->cbData == 0 ||
                data->cbData > MaximumCopyDataBytes ||
                (data->cbData % sizeof(wchar_t)) != 0)
            {
                return paths;
            }

            auto const* const characters = static_cast<wchar_t const*>(data->lpData);
            auto const count = data->cbData / sizeof(wchar_t);

            // The sender is another process, so the buffer is walked by length rather than by
            // trusting it to be terminated.
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
