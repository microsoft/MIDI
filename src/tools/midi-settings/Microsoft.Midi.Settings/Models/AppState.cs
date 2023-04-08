using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Midi.Settings.SdkMock;

namespace Microsoft.Midi.Settings.Models;

// TODO: Make this an injected singleton
public class AppState
{
    private static AppState? _current;
    private MidiSession? _midiSession;

    private AppState()
    {
    }

    public static AppState Current
    {
        get
        {
            if (_current is null)
                _current = new AppState();

            return _current;
        }
    }

    // static because the settings app is only opening one session, but an 
    // app can have more than one MidiSession
    public MidiSession? MidiSession
    {
        get
        {
            return _midiSession; 
        }
    }

    public bool HasActiveMidiSession
    {
        get
        {
            return _midiSession != null;
        }
    }

    public void CreateMidiSession()
    {
        // todo: set session name etc.

        _midiSession = new MidiSession();
    }


}
