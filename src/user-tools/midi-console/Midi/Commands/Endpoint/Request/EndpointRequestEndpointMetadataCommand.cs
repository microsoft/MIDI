using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
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
            // build the message

            UInt64 timestamp = 0;   // send immediately
            byte umpVersionMajor = settings.UmpVersionMajor;
            byte umpVersionMinor = settings.UmpVersionMinor;

            MidiEndpointDiscoveryFilterFlags flags = MidiEndpointDiscoveryFilterFlags.None;

            if (settings.RequestAll || settings.RequestEndpointInfo) flags |= MidiEndpointDiscoveryFilterFlags.RequestEndpointInformation;
            if (settings.RequestAll || settings.RequestDeviceIdentity) flags |= MidiEndpointDiscoveryFilterFlags.RequestDeviceIdentity;
            if (settings.RequestAll || settings.RequestEndpointName) flags |= MidiEndpointDiscoveryFilterFlags.RequestEndpointName;
            if (settings.RequestAll || settings.RequestProductInstanceId) flags |= MidiEndpointDiscoveryFilterFlags.RequestProductInstanceId;
            if (settings.RequestAll || settings.RequestStreamConfiguration) flags |= MidiEndpointDiscoveryFilterFlags.RequestStreamConfiguration;

            var message = MidiStreamMessageBuilder.BuildEndpointDiscoveryMessage(timestamp, umpVersionMajor, umpVersionMinor, flags);

            return (int)EndpointMessageSender.OpenTemporaryConnectionAndSendMidiMessage(settings.EndpointDeviceId, message);
        }
    }
}