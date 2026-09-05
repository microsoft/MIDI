// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    struct EnumEndpointsOptions
    {
        bool ShowEndpointId{ false };
        bool IncludeDiagnosticLoopback{ false };
        bool IncludeAll{ false };
        bool Verbose{ false };
    };

    struct EnumLegacyOptions
    {
        std::string Direction{ "All" };
        bool IncludePortId{ false };
    };

    struct EnumSessionsOptions
    {
        bool All{ false };
        bool Verbose{ false };
    };

    struct EnumTransportsOptions
    {
        bool Verbose{ false };
    };

    int RunEnumEndpointsCommand(_In_ EnumEndpointsOptions const& options);
    int RunEnumLegacyCommand(_In_ EnumLegacyOptions const& options);
    int RunEnumSessionsCommand(_In_ EnumSessionsOptions const& options);
    int RunEnumTransportsCommand(_In_ EnumTransportsOptions const& options);
    int RunEnumPropertyKeysCommand();
}
