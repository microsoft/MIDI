// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ServiceFirewall.h"
#include "StringResources.h"

namespace res = ::midinetworksetup::resources;

namespace midinetworksetup::firewall
{
    namespace
    {
        // The elevated copy is started with exactly this switch and a list of network types, like
        // "domain,private".
        constexpr wchar_t AllowSwitch[] = L"--allow-midi-service-through-firewall";

        // The MIDI service, wherever it runs from: the rule names its executable, which the
        // service control manager is asked for.
        ::midiapp::firewall::FirewallTarget ServiceTarget()
        {
            ::midiapp::firewall::FirewallTarget target{};

            target.ServiceName = L"MidiSrv";
            target.ProgramFileName = L"\\midisrv.exe";
            target.RuleName = res::GetString(L"FirewallRuleName");
            target.RuleDescription = res::GetString(L"FirewallRuleDescription");
            target.AllowSwitch = AllowSwitch;

            return target;
        }
    }

    FirewallState QueryState() noexcept
    {
        try
        {
            return ::midiapp::firewall::QueryState(ServiceTarget());
        }
        catch (...)
        {
            return FirewallState{};
        }
    }

    _Use_decl_annotations_
    HRESULT RequestAllow(HWND const owner, long const networks) noexcept
    {
        try
        {
            return ::midiapp::firewall::RequestAllow(owner, ServiceTarget(), networks);
        }
        CATCH_RETURN();
    }

    _Use_decl_annotations_
    bool TryRunAllowRequest(int& exitCode) noexcept
    {
        exitCode = 0;

        // Checked before anything else, so an ordinary launch never loads a string here.
        if (!::midiapp::firewall::IsAllowRequest(AllowSwitch))
        {
            return false;
        }

        HRESULT result{ S_OK };
        long networks{ 0 };

        try
        {
            ::midiapp::firewall::TryRunAllowRequest(ServiceTarget(), result, networks);
        }
        catch (...)
        {
            result = E_UNEXPECTED;
        }

        // the event name and level are part of the event's metadata, so each needs its own write
        if (SUCCEEDED(result))
        {
            TraceLoggingWrite(
                MidiNetworkSetupTelemetryProvider::Provider(),
                MIDI_NETSETUP_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingWideString(L"Allowed the MIDI service through Windows Firewall.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                TraceLoggingLong(networks, "networks"));
        }
        else
        {
            TraceLoggingWrite(
                MidiNetworkSetupTelemetryProvider::Provider(),
                MIDI_NETSETUP_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Unable to allow the MIDI service through Windows Firewall.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                TraceLoggingLong(networks, "networks"),
                TraceLoggingHResult(result, MIDI_NETSETUP_TRACE_HRESULT_FIELD));
        }

        exitCode = static_cast<int>(result);

        return true;
    }
}
