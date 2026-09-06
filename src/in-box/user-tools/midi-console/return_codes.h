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
    // These values are a contract with scripts, so they match the shipping console exactly.
    enum class ReturnCode : int
    {
        Success = 0,

        ErrorUserCanceled = 1,

        ErrorWinRTTypeActivationFailure = 10,
        ErrorMidiServicesSdkNotInstalled = 12,

        ErrorCreatingSession = 100,

        ErrorNoEndpointsFound = 200,
        ErrorCreatingEndpointConnection = 210,
        ErrorOpeningEndpointConnection = 212,

        ErrorMalformedUmp = 300,

        ErrorServiceNotAvailable = 800,
        ErrorMidiServicesFeatureNotEnabled = 801,

        ErrorInsufficientPermissions = 850,

        ErrorNotImplemented = 998,
        ErrorGeneralFailure = 999
    };

    constexpr int AsExitCode(_In_ ReturnCode code) noexcept
    {
        return static_cast<int>(code);
    }
}
