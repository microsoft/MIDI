// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
{
    public enum MidiConsoleReturnCode : int
    {
        Success = 0,

        ErrorWinRTTypeActivationFailure = 10,
        ErrorMidiServicesSdkNotInstalled = 12,

        ErrorCreatingSession = 100,

        ErrorNoEndpointsFound = 200,
        ErrorCreatingEndpointConnection = 210,
        ErrorOpeningEndpointConnection = 212,

        ErrorMalformedUmp = 300,

        ErrorServiceNotAvailable = 800,

        ErrorInsufficientPermissions = 850,

        ErrorNotImplemented = 998,
        ErrorGeneralFailure = 999
    }
}
