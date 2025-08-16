// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.VoiceCommands;

namespace Microsoft.Midi.Settings.Contracts.Services;

public class MidiSettingsSearchResult
{
    // TODO: might want to change this to a path to an image
    public string Glyph;

    public string DisplayText;

    public string ResultType;

    public List<string> Keywords = [];

    public string DestinationKey;

    public object? Parameter = null;


    public void AddKeyword(string keyword)
    {
        var cleaned = keyword.ToLower().Trim();

        if (cleaned != string.Empty)
        {
            Keywords.Add(cleaned);
        }
    }

}


public interface IMidiSettingsSearchService
{
    public IList<MidiSettingsSearchResult> GetFilteredResults(string filterText);


    public void Refresh();

}
