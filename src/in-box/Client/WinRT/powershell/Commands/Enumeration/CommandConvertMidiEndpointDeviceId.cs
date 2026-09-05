// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Enumeration;

namespace WindowsMidiServices
{
    [Cmdlet(VerbsData.Convert, "MidiEndpointDeviceId")]
    [OutputType(typeof(string))]
    public class CommandConvertMidiEndpointDeviceId : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        public string EndpointDeviceId { get; set; } = string.Empty;

        [Parameter(Mandatory = false, ParameterSetName = "Short")]
        public SwitchParameter Short { get; set; }

        [Parameter(Mandatory = false, ParameterSetName = "Full")]
        public SwitchParameter Full { get; set; }

        [Parameter(Mandatory = false, ParameterSetName = "Normalize")]
        public SwitchParameter Normalize { get; set; }

        protected override void ProcessRecord()
        {
            var id = EndpointDeviceId.Trim();

            if (Short.IsPresent)
            {
                if (!MidiEndpointDeviceHelper.IsPossibleWindowsMidiServicesEndpointDeviceId(id))
                {
                    WriteError(new ErrorRecord(
                        new ArgumentException($"'{id}' is not a Windows MIDI Services endpoint device id."),
                        "NotAnEndpointDeviceId", ErrorCategory.InvalidArgument, id));

                    return;
                }

                WriteObject(MidiEndpointDeviceHelper.GetShortIdFromFullId(id));

                return;
            }

            if (Full.IsPresent)
            {
                var fullId = MidiEndpointDeviceHelper.GetFullIdFromShortId(id);

                if (string.IsNullOrEmpty(fullId))
                {
                    WriteError(new ErrorRecord(
                        new ArgumentException($"'{id}' could not be expanded into an endpoint device id."),
                        "NotAnEndpointDeviceId", ErrorCategory.InvalidArgument, id));

                    return;
                }

                WriteObject(fullId);

                return;
            }

            // Normalizing is the useful default: ids differ only by case and trailing slash, so
            // comparing raw strings from different sources otherwise gives false mismatches.
            WriteObject(MidiEndpointDeviceHelper.NormalizeFullId(id));
        }
    }
}
