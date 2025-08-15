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

namespace Microsoft.Midi.Settings.Contracts.Services
{
    public class MidiSettingsSearchResult
    {
        public string DisplayText;

        public IList<string> Keywords = new List<string>();

        public string DestinationKey;

        public object? Parameter;
    }


    public interface IMidiSettingsSearchService
    {
        public IList<MidiSettingsSearchResult> GetFilteredResults(string filterText);


    }
}
