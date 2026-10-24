// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "SysExPrompt.h"

#include "ContentProvenance.h"
#include "StringResources.h"
#include "SysExReview.h"

#include <commctrl.h>

#include <filesystem>

#pragma comment(lib, "comctl32.lib")

namespace resources = ::midiglass::resources;

namespace midiglass::sysexprompt
{
    namespace
    {
        constexpr int AllowButton = 101;
        constexpr int BlockButton = 102;

        constexpr size_t MaximumListedDevices = 12;
        constexpr size_t MaximumNameLength = 80;

        // A name from the layout file, as somebody else wrote it, with nothing in it that could
        // make it look like part of the question.
        std::wstring Shown(_In_ std::wstring const& name)
        {
            return midiapp::SanitizeProvenanceText(name, MaximumNameLength);
        }

        // The layout's name for the device and, when it says, the device it looks for.
        std::wstring DescribeDevice(_In_ glass::LayoutDocument const& document, _In_ std::wstring const& deviceName)
        {
            auto const name = Shown(deviceName);

            if (name.empty())
            {
                return std::wstring{ resources::GetString(L"SysExPromptNoDevice") };
            }

            for (auto const& device : document.Devices)
            {
                if (device.Name != deviceName)
                {
                    continue;
                }

                auto const looksFor = Shown(device.Match.TransportSuppliedEndpointName.empty()
                    ? device.Match.ParentDeviceName
                    : device.Match.TransportSuppliedEndpointName);

                if (!looksFor.empty() && looksFor != name)
                {
                    return std::wstring{ resources::FormatString(L"SysExPromptDeviceMatchFormat", name, looksFor) };
                }

                break;
            }

            return name;
        }
    }

    _Use_decl_annotations_
    bool AskToAllowSysEx(HWND owner, glass::LayoutDocument const& document, std::wstring const& filePath)
    {
        try
        {
            auto const devices = glass::SystemExclusiveDevices(document);

            std::wstring list{};

            for (size_t i = 0; i < devices.size() && i < MaximumListedDevices; ++i)
            {
                list += L"\u2022 ";
                list += DescribeDevice(document, devices[i]);
                list += L"\n";
            }

            if (devices.size() > MaximumListedDevices)
            {
                list += resources::FormatString(L"SysExPromptMoreFormat", std::to_wstring(devices.size() - MaximumListedDevices));
                list += L"\n";
            }

            auto layoutName = Shown(document.Name);

            if (layoutName.empty())
            {
                layoutName = std::filesystem::path{ filePath }.stem().wstring();
            }

            auto const title = resources::GetString(L"SysExPromptTitle");
            auto const instruction = resources::FormatString(L"SysExPromptInstructionFormat", layoutName);
            auto const content = std::wstring{ resources::GetString(L"SysExPromptIntro") } + L"\n\n" + list + L"\n" +
                std::wstring{ resources::GetString(L"SysExPromptWarning") };
            auto const allow = resources::GetString(L"SysExPromptAllow");
            auto const block = resources::GetString(L"SysExPromptBlock");

            TASKDIALOG_BUTTON buttons[]
            {
                { AllowButton, allow.c_str() },
                { BlockButton, block.c_str() },
            };

            TASKDIALOGCONFIG config{};
            config.cbSize = sizeof(config);
            config.hwndParent = owner;
            config.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
            config.pszWindowTitle = title.c_str();
            config.pszMainIcon = TD_WARNING_ICON;
            config.pszMainInstruction = instruction.c_str();
            config.pszContent = content.c_str();
            config.cButtons = ARRAYSIZE(buttons);
            config.pButtons = buttons;

            // The safe answer is the one Enter gives.
            config.nDefaultButton = BlockButton;

            int pressed{ 0 };

            if (FAILED(::TaskDialogIndirect(&config, &pressed, nullptr, nullptr)) || pressed != AllowButton)
            {
                return false;
            }

            glass::ApproveSysEx(document, filePath);
            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION_MSG("Unable to ask about a layout's SysEx.");
            return false;
        }
    }
}
