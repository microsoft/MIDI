// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OscText.h"
#include "StringResources.h"

namespace midipatchbay
{
    _Use_decl_annotations_
    winrt::hstring OscPresetName(OscPreset preset) noexcept
    {
        switch (preset)
        {
        case OscPreset::ReaperVirtualKeyboard: return resources::GetString(L"OscPresetReaper");
        case OscPreset::Paths: return resources::GetString(L"OscPresetPaths");
        case OscPreset::MidiType: return resources::GetString(L"OscPresetMidiType");
        case OscPreset::OpenStageControl: return resources::GetString(L"OscPresetOpenStageControl");
        case OscPreset::OscMidiOsc: return resources::GetString(L"OscPresetOscMidiOsc");
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring OscFormatName(OscFormat format) noexcept
    {
        switch (format)
        {
        case OscFormat::Paths: return resources::GetString(L"OscFormatPaths");
        case OscFormat::MidiType: return resources::GetString(L"OscFormatMidiType");
        case OscFormat::Arguments: return resources::GetString(L"OscFormatArguments");
        }

        return {};
    }

    _Use_decl_annotations_
    std::wstring OscRowLabel(OscRow const& row) noexcept
    {
        try
        {
            auto const address = OscRowAddressText(row);

            if (row.Format == OscFormat::MidiType)
            {
                return std::wstring{ resources::FormatString(L"OscRowMidiTypeFormat", address, static_cast<int32_t>(row.Group)) };
            }

            auto const preset = OscPresetOf(row);
            auto const kind = preset.has_value() ? OscPresetName(*preset) : OscFormatName(row.Format);

            return std::wstring{ resources::FormatString(L"OscRowFormat", address, kind) };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring OscNodeSubtitle(OscConnectionSettings const& settings) noexcept
    {
        try
        {
            auto const receives = OscConnectionReceives(settings);
            auto const sends = OscConnectionSends(settings);

            std::wstring destination{};

            if (sends)
            {
                destination = settings.SendHost.empty()
                    ? std::wstring{ resources::FormatString(L"OscLastSenderPortFormat", static_cast<int32_t>(settings.SendPort)) }
                    : std::format(L"{}:{}", settings.SendHost, settings.SendPort);
            }

            if (receives && sends)
            {
                return std::wstring{ resources::FormatString(L"OscSubtitleBothFormat", static_cast<int32_t>(settings.ReceivePort), destination) };
            }

            if (receives)
            {
                return std::wstring{ resources::FormatString(L"OscSubtitleReceiveFormat", static_cast<int32_t>(settings.ReceivePort)) };
            }

            if (sends)
            {
                return std::wstring{ resources::FormatString(L"OscSubtitleSendFormat", destination) };
            }

            return std::wstring{ resources::GetString(L"OscSubtitleNeither") };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring OscListeningText(OscConnectionSettings const& settings, std::optional<OscConnectionStatus> const& status) noexcept
    {
        try
        {
            if (!OscConnectionReceives(settings))
            {
                return std::wstring{ resources::GetString(L"OscListeningOff") };
            }

            auto const port = static_cast<int32_t>(settings.ReceivePort);

            if (!status.has_value())
            {
                return std::wstring{ resources::FormatString(L"OscListeningNotRoutingFormat", port) };
            }

            switch (status->Listening)
            {
            case OscListenState::EveryNetwork:
                return std::wstring{ resources::FormatString(L"OscListeningEveryNetworkFormat", port) };

            case OscListenState::ThisPcOnly:
                return std::wstring{ resources::FormatString(status->HeldByFirewall
                    ? L"OscListeningHeldFormat"
                    : L"OscListeningThisPcFormat", port) };

            case OscListenState::PortInUse:
                return status->PortOwner.empty()
                    ? std::wstring{ resources::FormatString(L"OscPortInUseFormat", port) }
                    : std::wstring{ resources::FormatString(L"OscPortInUseByFormat", port, status->PortOwner) };

            case OscListenState::Failed:
                return std::wstring{ resources::FormatString(L"OscListeningFailedFormat", port) };

            default:
                return std::wstring{ resources::GetString(L"OscListeningOff") };
            }
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring OscLastHeardText(OscConnectionStatus const& status) noexcept
    {
        try
        {
            if (status.LastHeardWhen == 0)
            {
                return std::wstring{ resources::GetString(L"OscNothingHeardYet") };
            }

            FILETIME now{};
            ::GetSystemTimePreciseAsFileTime(&now);

            auto const nowTicks = static_cast<int64_t>((static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);
            auto const seconds = static_cast<int32_t>((std::max)(int64_t{ 0 }, (nowTicks - status.LastHeardWhen) / 10'000'000));

            if (seconds < 2)
            {
                return std::wstring{ resources::FormatString(L"OscLastHeardJustNowFormat", status.LastHeardFrom) };
            }

            if (seconds < 120)
            {
                return std::wstring{ resources::FormatString(L"OscLastHeardSecondsFormat", status.LastHeardFrom, seconds) };
            }

            return std::wstring{ resources::FormatString(L"OscLastHeardMinutesFormat", status.LastHeardFrom, seconds / 60) };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool WantsOscOnEveryNetwork(PatchDocument const& patch) noexcept
    {
        return std::any_of(patch.Endpoints.begin(), patch.Endpoints.end(), [](PatchEndpoint const& endpoint)
            {
                return endpoint.IsOsc() && endpoint.Osc->ReceivePort != 0 && endpoint.Osc->ListenOn == OscListenOn::EveryNetwork;
            });
    }

    _Use_decl_annotations_
    bool OscFirewallHolds(PatchDocument const& patch) noexcept
    {
        using midiapp::firewall::FirewallStatus;

        if (!WantsOscOnEveryNetwork(patch) || !OscHub::Current().IsHeldByFirewall())
        {
            return false;
        }

        auto const status = OscHub::Current().Firewall().Status;

        return status == FirewallStatus::Closed || status == FirewallStatus::ClosedByRule || status == FirewallStatus::ClosedForAll;
    }
}
