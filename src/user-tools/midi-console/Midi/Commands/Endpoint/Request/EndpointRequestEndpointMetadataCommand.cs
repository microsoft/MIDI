// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



//using Microsoft.Windows.Devices.Midi2.Initialization;
using Microsoft.Windows.Devices.Midi2.Messages;

namespace Microsoft.Midi.ConsoleApp
{
    internal class EndpointRequestEndpointMetadataCommand : Command<EndpointRequestEndpointMetadataCommand.Settings>
    {
        public sealed class Settings : EndpointRequestCommandSettings
        {
            [LocalizedDescription("ParameterRequestEndpointMetadataAll")]
            [CommandOption("-a|--all")]
            [DefaultValue(false)]
            public bool RequestAll { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataEndpointInfoNotification")]
            [CommandOption("-i|--endpoint-info")]
            [DefaultValue(true)]
            public bool RequestEndpointInfo { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataDeviceIdentityNotification")]
            [CommandOption("-d|--device-identity")]
            [DefaultValue(false)]
            public bool RequestDeviceIdentity { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataEndpointNameNotification")]
            [CommandOption("-n|--name")]
            [DefaultValue(false)]
            public bool RequestEndpointName { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataEndpointProductInstanceIdNotification")]
            [CommandOption("-p|--product-instance-id")]
            [DefaultValue(false)]
            public bool RequestProductInstanceId { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataEndpointStreamConfigurationNotification")]
            [CommandOption("-s|--stream-configuration")]
            [DefaultValue(false)]
            public bool RequestStreamConfiguration { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataEndpointUmpVersionMajor")]
            [CommandOption("-j|--ump-version-major")]
            [DefaultValue((byte)1)]
            public byte UmpVersionMajor { get; set; }

            [LocalizedDescription("ParameterRequestEndpointMetadataEndpointUmpVersionMinor")]
            [CommandOption("-m|--ump-version-minor")]
            [DefaultValue((byte)1)]
            public byte UmpVersionMinor { get; set; }

        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate at least one of the options is true 

            return ValidationResult.Success();
        }


        // TODO: Tons of strings in here that need to be localized

        public override int Execute(CommandContext context, Settings settings)
        {
            //if (!MidiServicesInitializer.EnsureServiceAvailable())
            //{
            //    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
            //    return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            //}

            // build the message

            UInt64 timestamp = 0;   // send immediately
            byte umpVersionMajor = settings.UmpVersionMajor;
            byte umpVersionMinor = settings.UmpVersionMinor;

            MidiEndpointDiscoveryRequests flags = MidiEndpointDiscoveryRequests.None;

            if (settings.RequestAll || settings.RequestEndpointInfo) flags |= MidiEndpointDiscoveryRequests.RequestEndpointInfo;
            if (settings.RequestAll || settings.RequestDeviceIdentity) flags |= MidiEndpointDiscoveryRequests.RequestDeviceIdentity;
            if (settings.RequestAll || settings.RequestEndpointName) flags |= MidiEndpointDiscoveryRequests.RequestEndpointName;
            if (settings.RequestAll || settings.RequestProductInstanceId) flags |= MidiEndpointDiscoveryRequests.RequestProductInstanceId;
            if (settings.RequestAll || settings.RequestStreamConfiguration) flags |= MidiEndpointDiscoveryRequests.RequestStreamConfiguration;

            var message = MidiStreamMessageBuilder.BuildEndpointDiscoveryMessage(timestamp, umpVersionMajor, umpVersionMinor, flags);

            return (int)EndpointMessageSender.OpenTemporaryConnectionAndSendMidiMessage(settings.EndpointDeviceId, message);
        }
    }
}