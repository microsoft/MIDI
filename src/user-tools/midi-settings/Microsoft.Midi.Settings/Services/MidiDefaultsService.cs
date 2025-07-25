﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;

namespace Microsoft.Midi.Settings.Services
{
    public class MidiDefaultsService : IMidiDefaultsService
    {
        const string DefaultLoopbackAUniqueId = "DEFAULT_LOOPBACK_A";
        const string DefaultLoopbackBUniqueId = "DEFAULT_LOOPBACK_B";

        public string GetDefaultMidiConfigName()
        {
            return MidiConfigConstants.DefaultConfigurationName;
        }
        public string GetDefaultMidiConfigFileName()
        {
            return MidiConfigConstants.DefaultConfigurationFileName;
        }


        public MidiLoopbackEndpointCreationConfig GetDefaultLoopbackCreationConfig()
        {
            var endpointA = new MidiLoopbackEndpointDefinition();
            var endpointB = new MidiLoopbackEndpointDefinition();

            // if endpoint A or B names are empty, do not close. display an error

            // if endpoint A or B unique ids are empty, do not close. display suggestion to generate them
            // todo: need to limit to alpha plus just a couple other characters, and only 32 in length

            endpointA.Name = "Default Loopback A";
            endpointB.Name = "Default Loopback B";

            endpointA.UniqueId = DefaultLoopbackAUniqueId;
            endpointB.UniqueId = DefaultLoopbackBUniqueId;

            // descriptions are optional
            endpointA.Description = "Default loopback endpoint for use by applications. This is the A-side of the loopback pair.";
            endpointB.Description = "Default loopback endpoint for use by applications. This is the B-side of the loopback pair.";

            // TODO: entries for the default groups to create, and their gtb names

            var associationId = GuidHelper.CreateNewGuid();

            var creationConfig = new MidiLoopbackEndpointCreationConfig(associationId, endpointA, endpointB);

            return creationConfig;
        }

        public bool DoesDefaultLoopbackAlreadyExist()
        {
            // TODO: Check to see if an endpoint with the unique ids here already exists

            return false;
        }
    }
}
