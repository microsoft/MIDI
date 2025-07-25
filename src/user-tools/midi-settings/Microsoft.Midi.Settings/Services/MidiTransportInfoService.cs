﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    class MidiTransportInfoService : IMidiTransportInfoService
    {
        public MidiServiceTransportPluginInfo GetTransportForCode(string transportCode)
        {
            var codes = MidiReporting.GetInstalledTransportPlugins();

            return codes.Where(p => p.TransportCode.ToUpper() == transportCode.ToUpper()).FirstOrDefault<MidiServiceTransportPluginInfo>();
        }
    }
}
