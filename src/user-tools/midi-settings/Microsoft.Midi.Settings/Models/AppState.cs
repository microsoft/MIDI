// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Resources;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.Popups;

using Microsoft.Windows.Devices.Midi2.Utilities.Update;
using Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation;


namespace Microsoft.Midi.Settings.Models;

// TODO: Make this an injected singleton
public class AppState
{
    private static AppState? _current;


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


    public string GetCurrentSetupFileName()
    {
        // read from registry

        var val = (string)Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services", "CurrentConfig", "")!;

        return val;
    }

    // TODO: These should be in an Update service, not the state class
    public string GetInstalledSdkVersionString()
    {
        var version = MidiRuntimeInformation.GetInstalledVersion();

        return version.ToString();
    }



}
