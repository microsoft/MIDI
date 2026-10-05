// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "FileDialogs.h"

namespace midisoundfontsynth
{
    _Use_decl_annotations_
    std::wstring PickSoundFontFile(
        HWND owner,
        std::wstring const& startFolder,
        std::wstring const& title,
        std::wstring const& filterName,
        std::wstring const& allFilesName) noexcept
    {
        try
        {
            auto dialog = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog, CLSCTX_INPROC_SERVER);

            COMDLG_FILTERSPEC const filters[]
            {
                { filterName.c_str(), L"*.sf2" },
                { allFilesName.c_str(), L"*.*" },
            };

            THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            THROW_IF_FAILED(dialog->SetFileTypeIndex(1));
            THROW_IF_FAILED(dialog->SetTitle(title.c_str()));

            DWORD options{ 0 };
            THROW_IF_FAILED(dialog->GetOptions(&options));

            // A file on a disk, local or on the network. Nothing a shell namespace extension
            // would have to make up on the fly.
            THROW_IF_FAILED(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST));

            if (!startFolder.empty())
            {
                wil::com_ptr<IShellItem> folder{};

                if (SUCCEEDED(::SHCreateItemFromParsingName(startFolder.c_str(), nullptr, IID_PPV_ARGS(&folder))))
                {
                    (void)dialog->SetFolder(folder.get());
                }
            }

            auto const result = dialog->Show(owner);

            if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            {
                return {};
            }

            THROW_IF_FAILED(result);

            wil::com_ptr<IShellItem> item{};
            THROW_IF_FAILED(dialog->GetResult(&item));

            wil::unique_cotaskmem_string path{};
            THROW_IF_FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path));

            return path ? std::wstring{ path.get() } : std::wstring{};
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show the open dialog.")

        return {};
    }
}
