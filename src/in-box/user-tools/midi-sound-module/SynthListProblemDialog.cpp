// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SynthListProblemDialog.h"

#include "SingleInstance.h"
#include "StringResources.h"

namespace midisoundmodule
{
    namespace
    {
        constexpr int ShowFileButton = 100;

        // Windows' own words for an error, without the line break it ends with.
        std::wstring SystemMessage(_In_ uint32_t error)
        {
            wil::unique_hlocal_string message{};

            auto const length = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error, 0, reinterpret_cast<PWSTR>(message.put()), 0, nullptr);

            if (length == 0 || !message)
            {
                return std::format(L"0x{:08X}", error);
            }

            std::wstring text{ message.get(), length };

            while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
            {
                text.pop_back();
            }

            return text;
        }

        std::wstring ReasonText(_In_ SynthList const& list)
        {
            namespace res = resources;

            switch (list.Problem)
            {
            case SynthListProblem::CannotOpen:
                return std::wstring{ res::FormatString(L"SynthListCannotOpenFormat", SystemMessage(list.SystemError)) };

            case SynthListProblem::TooLarge:
                return std::wstring{ res::GetString(L"SynthListTooLarge") };

            case SynthListProblem::NotJson:
                return std::wstring{ res::GetString(L"SynthListNotJson") };

            case SynthListProblem::NoSynthList:
                return std::wstring{ res::GetString(L"SynthListNoList") };

            case SynthListProblem::NewerVersion:
                return std::wstring{ res::GetString(L"SynthListNewerVersion") };

            case SynthListProblem::TooManySynths:
                return std::wstring{ res::FormatString(L"SynthListTooManyFormat", MaximumSynths) };

            case SynthListProblem::EntryNotASynth:
                return std::wstring{ res::FormatString(L"SynthListEntryNotASynthFormat", list.Entry) };

            case SynthListProblem::EntryId:
                return std::wstring{ res::FormatString(L"SynthListEntryIdFormat", list.Entry) };

            case SynthListProblem::EntrySoundBank:
                return std::wstring{ res::FormatString(L"SynthListEntrySoundBankFormat", list.Entry) };

            case SynthListProblem::EntryDuplicateId:
                return std::wstring{ res::FormatString(L"SynthListEntryDuplicateIdFormat", list.Entry) };

            case SynthListProblem::None:
            default:
                return {};
            }
        }

        void ShowInExplorer(_In_ std::wstring const& path) noexcept
        {
            try
            {
                wil::unique_any<PIDLIST_ABSOLUTE, decltype(&::ILFree), ::ILFree> item{ ::ILCreateFromPathW(path.c_str()) };

                if (!item || FAILED(::SHOpenFolderAndSelectItems(item.get(), 0, nullptr, 0)))
                {
                    MIDI_SF2SYNTH_LOG_WARNING(L"Unable to show the synth list in File Explorer.");
                }
            }
            MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show the synth list in File Explorer.")
        }

        HRESULT CALLBACK OnDialogEvent(
            _In_ HWND dialog,
            _In_ UINT notification,
            _In_ WPARAM wParam,
            _In_ LPARAM /*lParam*/,
            _In_ LONG_PTR data) noexcept
        {
            // Launching the app again, perhaps because this went behind another window, brings it back
            // the way it brings back the main window. Nothing needs unpublishing afterward: wWinMain
            // lets go of the single instance as soon as this returns.
            if (notification == TDN_CREATED)
            {
                ::midiapp::SingleInstance::PublishMainWindow(dialog);
            }

            if (notification == TDN_BUTTON_CLICKED && wParam == ShowFileButton && data != 0)
            {
                ShowInExplorer(*reinterpret_cast<std::wstring const*>(data));

                // Stays open, so the reason can still be read beside the file.
                return S_FALSE;
            }

            return S_OK;
        }
    }

    _Use_decl_annotations_
    void ShowSynthListProblem(SynthList const& list, std::wstring const& path) noexcept
    {
        try
        {
            namespace res = resources;

            auto const title = res::GetString(L"AppDisplayName");
            auto const instruction = res::GetString(L"SynthListProblemInstruction");
            auto const content = res::FormatString(L"SynthListProblemContentFormat", ReasonText(list), path);
            auto const showFile = res::GetString(L"SynthListProblemShowFile");
            auto const close = res::GetString(L"SynthListProblemClose");

            TASKDIALOG_BUTTON const buttons[]
            {
                { ShowFileButton, showFile.c_str() },
                { IDCLOSE, close.c_str() },
            };

            TASKDIALOGCONFIG config{};
            config.cbSize = sizeof(config);
            config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
            config.pszWindowTitle = title.c_str();
            config.pszMainIcon = TD_ERROR_ICON;
            config.pszMainInstruction = instruction.c_str();
            config.pszContent = content.c_str();
            config.cButtons = ARRAYSIZE(buttons);
            config.pButtons = buttons;
            config.nDefaultButton = IDCLOSE;
            config.pfCallback = OnDialogEvent;
            config.lpCallbackData = reinterpret_cast<LONG_PTR>(&path);

            (void)::TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to show why the synth list can't be read.")
    }
}
