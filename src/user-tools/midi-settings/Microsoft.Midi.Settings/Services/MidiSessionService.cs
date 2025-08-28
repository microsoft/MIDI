// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;


namespace Microsoft.Midi.Settings.Services;

public class MidiSessionService : IMidiSessionService
{
    public MidiSessionService()
    {
        _session = MidiSession.Create("AppSessionName".GetLocalized());
    }

    private MidiSession? _session;
    public MidiSession? Session { get { return _session; } }


    public MidiEndpointConnection? GetConnection(string endpointDeviceId)
    {
        var cleanId = endpointDeviceId.ToLower().Trim();

        if (string.IsNullOrEmpty(cleanId))
        {
            return null;
        }

        if (_session == null)
        {
            return null;
        }

        // if the connection is already open, return that. Otherwise, open it

        foreach (var connection in _session.Connections)
        {
            if (connection.Value.ConnectedEndpointDeviceId == cleanId)
            {
                return connection.Value;
            }
        }

        // connection wasn't found, so create and open it

        var conn = _session.CreateEndpointConnection(cleanId);
        conn.Open();

        return conn;
    }
}
