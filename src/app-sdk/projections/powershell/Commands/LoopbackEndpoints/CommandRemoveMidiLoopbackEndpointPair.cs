// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Microsoft.Windows.Devices.Midi2.Reporting;
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Remove, "MidiLoopbackEndpointPair")]
    public class CommandRemoveMidiLoopbackEndpointPair : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public required Guid LoopbackEndpointPairAssociationId
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            try 
            {
                var sdkSessions = MidiReporting.GetActiveSessions();
            }
            catch
            {
                throw new Exception("No midi session found. Make sure to first run \"Start-MidiSession\"");
            }

            var removalConfig = new MidiLoopbackEndpointRemovalConfig(LoopbackEndpointPairAssociationId);

           
            try
            {
                var result = MidiLoopbackEndpointManager.RemoveTransientLoopbackEndpoints(removalConfig);

                WriteObject(result);
            }
            catch (Exception e)
            {
                ErrorRecord err = new ErrorRecord(
                    e,
                    "LoopbackRemovalError",
                    ErrorCategory.DeviceError,
                    null);
                WriteError(err);
                return;
            }
        }
    }


}
