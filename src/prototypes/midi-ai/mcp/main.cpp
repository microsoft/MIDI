// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. midi-mcp-spike [--app patchbay|glass|all] [--patch-folder <path>]
//                           [--layout-folder <path>] [--midiglass <path to midiglass.exe>]
//
// An MCP server on stdin and stdout for an assistant the customer already uses (GitHub Copilot,
// Claude and others). A shipping version would be one server per app, most likely the app's own
// exe started with a switch, the way midiglass --thumbnail already works.

#include "pch.h"
#include "McpServer.h"
#include "EndpointTools.h"
#include "PatchbayTools.h"
#include "GlassTools.h"
#include "ToolText.h"

namespace
{
    constexpr wchar_t PrototypeVersion[] = L"0.1.0-prototype";

    constexpr wchar_t PatchbayInstructions[] =
        L"These tools turn what the customer asks for into a MIDI Patchbay patch: which devices connect to which, "
        L"and what is filtered or changed on the way. Work in this order. Call list_midi_endpoints to see what is "
        L"connected. Ask the customer about anything that is not clear, such as which keyboard, which channel or "
        L"where to split. Call preview_patch and read its summary back to the customer in plain words. Call "
        L"save_patch_draft only after they agree. A draft never routes by itself: the customer turns routing on in "
        L"MIDI Patchbay. Use the endpoint and group names the tools give you, and never invent an endpoint id.";

    constexpr wchar_t GlassInstructions[] =
        L"These tools design MIDI Glass layouts: touch control surfaces that send MIDI to the customer's devices. "
        L"Work in this order. Call list_midi_endpoints and list_glass_controls. Ask the customer what the surface is "
        L"for and what it should control. Call preview_layout, look at the picture it returns and fix what looks wrong, "
        L"then show it to the customer. Call save_layout_draft only after they agree. Nothing is sent to a device until "
        L"the customer runs the layout in MIDI Glass.";

    void Log(std::wstring const& text) noexcept
    {
        midimcp::LogLine(text);
    }
}

int __cdecl wmain(int argc, wchar_t** argv)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        std::wstring app{ L"all" };
        midimcp::PatchbayToolOptions patchbay{};
        midimcp::GlassToolOptions glass{};

        for (int i = 1; i < argc; i++)
        {
            std::wstring_view const argument{ argv[i] };
            auto const hasValue = i + 1 < argc;

            if (argument == L"--app" && hasValue)
            {
                app = argv[++i];
            }
            else if (argument == L"--patch-folder" && hasValue)
            {
                patchbay.PatchFolder = argv[++i];
            }
            else if (argument == L"--layout-folder" && hasValue)
            {
                glass.LayoutFolder = argv[++i];
            }
            else if (argument == L"--midiglass" && hasValue)
            {
                glass.MidiGlassExe = argv[++i];
            }
            else
            {
                Log(L"unknown argument " + std::wstring{ argument });
                return 2;
            }
        }

        auto const wantPatchbay = app == L"patchbay" || app == L"all";
        auto const wantGlass = app == L"glass" || app == L"all";

        if (!wantPatchbay && !wantGlass)
        {
            Log(L"--app is patchbay, glass or all");
            return 2;
        }

        midimcp::ServerIdentity identity{};
        identity.Version = PrototypeVersion;

        if (wantPatchbay && wantGlass)
        {
            identity.Name = L"windows-midi-tools-prototype";
            identity.Title = L"Windows MIDI Patchbay and MIDI Glass (prototype)";
            identity.Instructions = std::wstring{ PatchbayInstructions } + L"\n\n" + GlassInstructions;
        }
        else if (wantPatchbay)
        {
            identity.Name = L"windows-midi-patchbay-prototype";
            identity.Title = L"Windows MIDI Patchbay (prototype)";
            identity.Instructions = PatchbayInstructions;
        }
        else
        {
            identity.Name = L"windows-midi-glass-prototype";
            identity.Title = L"Windows MIDI Glass (prototype)";
            identity.Instructions = GlassInstructions;
        }

        midimcp::McpServer server{ identity };

        server.AddTool(midimcp::MakeListEndpointsTool());

        if (wantPatchbay)
        {
            for (auto& tool : midimcp::MakePatchbayTools(patchbay))
            {
                server.AddTool(std::move(tool));
            }
        }

        if (wantGlass)
        {
            for (auto& tool : midimcp::MakeGlassTools(glass))
            {
                server.AddTool(std::move(tool));
            }
        }

        Log(L"serving " + app + L" on stdio");

        return server.Run(::GetStdHandle(STD_INPUT_HANDLE), ::GetStdHandle(STD_OUTPUT_HANDLE));
    }
    catch (winrt::hresult_error const& error)
    {
        Log(L"failed to start: " + std::wstring{ error.message() });
    }
    catch (std::exception const& error)
    {
        Log(L"failed to start: " + midimcp::Utf8ToWide(error.what()));
    }
    catch (...)
    {
        Log(L"failed to start");
    }

    return 1;
}
