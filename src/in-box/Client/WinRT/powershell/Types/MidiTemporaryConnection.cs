// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace WindowsMidiServices
{
    // A session and connection opened for one command, for the cmdlets which take an endpoint
    // device id in place of a connection. Disposing it closes both.
    internal sealed class MidiTemporaryConnection : IDisposable
    {
        private Windows.Devices.Midi2.MidiSession? _session;

        public Windows.Devices.Midi2.MidiEndpointConnection Connection { get; }

        public MidiTemporaryConnection(
            Windows.Devices.Midi2.MidiSession session,
            Windows.Devices.Midi2.MidiEndpointConnection connection)
        {
            _session = session;
            Connection = connection;
        }

        public void Dispose()
        {
            // Closing the session closes the connection with it.
            Interlocked.Exchange(ref _session, null)?.Dispose();
        }
    }
}
