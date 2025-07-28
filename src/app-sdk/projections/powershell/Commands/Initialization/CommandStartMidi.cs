// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Initialization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Management.Automation;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    // there's an initialize verb, but no corresponding
    // terminate or shutdown, so we'll just use start/stop

    [Cmdlet(VerbsLifecycle.Start, "Midi")]
    public class InitializeCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            if (Initializer.CreateInitializer())
            {
                if (Initializer.Initialize())
                {
                    WriteVerbose("MIDI SDK: " + Initializer.GetSdkDescription());
                    WriteVerbose("MIDI Initialized.");
                }
                else
                {
                    // unable to initialize
                    //WriteError(new ErrorRecord())''
                }
            }
            else
            {
                // unable to create initializer
            }

        }

    }

}
