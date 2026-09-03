// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;

namespace WindowsMidiServices
{
    // Reports the groups an endpoint actually uses, taken from its declared function blocks
    // when it has them, and from its group terminal blocks when it does not.
    [Cmdlet(VerbsCommon.Get, "MidiEndpointGroup")]
    [OutputType(typeof(MidiEndpointGroupInfo))]
    public class CommandGetMidiEndpointGroup : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true)]
        [ValidateNotNullOrWhiteSpace]
        public string EndpointDeviceId { get; set; } = string.Empty;

        // Function blocks may be declared but inactive. They are hidden unless asked for,
        // because an inactive group carries no traffic.
        [Parameter]
        public SwitchParameter IncludeInactive { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var device = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(EndpointDeviceId);

            if (device is null)
            {
                WriteNonTerminating(
                    new ItemNotFoundException($"No MIDI endpoint was found with the identifier \"{EndpointDeviceId}\"."),
                    "MidiEndpointNotFound",
                    ErrorCategory.ObjectNotFound,
                    EndpointDeviceId);

                return;
            }

            var functionBlocks = device.GetDeclaredFunctionBlocks();

            if (functionBlocks is not null && functionBlocks.Count > 0)
            {
                foreach (var block in functionBlocks)
                {
                    if (!block.IsActive && !IncludeInactive.IsPresent)
                    {
                        continue;
                    }

                    EmitGroups(
                        device.EndpointDeviceId,
                        block.FirstGroup.Index,
                        block.GroupCount,
                        block.Name,
                        DirectionName(block.Direction),
                        "FunctionBlock",
                        block.Number,
                        block.IsActive);
                }

                return;
            }

            var terminalBlocks = device.GetGroupTerminalBlocks();

            if (terminalBlocks is null)
            {
                return;
            }

            foreach (var block in terminalBlocks)
            {
                EmitGroups(
                    device.EndpointDeviceId,
                    block.FirstGroup.Index,
                    block.GroupCount,
                    block.Name,
                    DirectionName(block.Direction),
                    "GroupTerminalBlock",
                    block.Number,
                    true);
            }
        }

        private void EmitGroups(
            string endpointDeviceId,
            byte firstGroupIndex,
            byte groupCount,
            string name,
            string direction,
            string blockKind,
            byte blockNumber,
            bool isActive)
        {
            for (var offset = 0; offset < groupCount; offset++)
            {
                var index = firstGroupIndex + offset;

                if (!MidiGroup.IsValidIndex((byte)index))
                {
                    continue;
                }

                WriteObject(new MidiEndpointGroupInfo
                {
                    GroupIndex = (byte)index,
                    GroupNumber = (byte)(index + 1),
                    Name = name,
                    Direction = direction,
                    BlockKind = blockKind,
                    BlockNumber = blockNumber,
                    IsActive = isActive,
                    EndpointDeviceId = endpointDeviceId
                });
            }
        }

        private static string DirectionName(MidiFunctionBlockDirection direction) => direction switch
        {
            MidiFunctionBlockDirection.BlockInput => "Input",
            MidiFunctionBlockDirection.BlockOutput => "Output",
            MidiFunctionBlockDirection.Bidirectional => "Bidirectional",
            _ => "Undefined",
        };

        private static string DirectionName(MidiGroupTerminalBlockDirection direction) => direction switch
        {
            MidiGroupTerminalBlockDirection.BlockInput => "Input",
            MidiGroupTerminalBlockDirection.BlockOutput => "Output",
            _ => "Bidirectional",
        };
    }

}
