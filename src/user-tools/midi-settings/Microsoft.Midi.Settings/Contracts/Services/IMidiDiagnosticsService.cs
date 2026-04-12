// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Microsoft.Midi.Settings.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services;

public class FoundRegistryEntry
{
    public string Name;
    public string Value;
    public bool HasError;
    public string Comment;
}

public interface IMidiDiagnosticsService
{
    bool CaptureMidiDiagOutputToNotepad();

    bool MidiFixReg();

    bool RestoreInBoxComponentRegistrations();

    string CaptureMidiLogsToFile();


    List<FoundRegistryEntry> GetDrivers32MidiEntries();

    List<FoundRegistryEntry> GetDrivers32WOWMidiEntries();

    //bool LaunchRegeditWithDrivers32WOWLocation();
    //bool LaunchRegeditWithDrivers32Location();
}