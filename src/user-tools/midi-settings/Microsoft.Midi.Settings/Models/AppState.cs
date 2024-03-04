using System;
using System.Collections.Generic;
using System.Linq;
using System.Resources;
using System.Text;
using System.Threading.Tasks;


namespace Microsoft.Midi.Settings.Models;

// TODO: Make this an injected singleton
public class AppState
{
    private static AppState? _current;
    private MidiSession? _midiSession;

    private AppState()
    {
        _midiSession = MidiSession.CreateSession("AppSessionName".GetLocalized());
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


}
