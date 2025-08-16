using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Strings.en_us;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
