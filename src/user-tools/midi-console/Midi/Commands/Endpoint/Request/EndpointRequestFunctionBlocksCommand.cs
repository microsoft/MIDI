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
    internal class EndpointRequestFunctionBlocksCommand : Command<EndpointRequestFunctionBlocksCommand.Settings>
    {
        public sealed class Settings : EndpointRequestCommandSettings
        {
            [LocalizedDescription("ParameterRequestFunctionBlocksAll")]
            [CommandOption("-a|--all")]
            [DefaultValue(false)]
            public bool RequestAll { get; set; }


            [LocalizedDescription("ParameterRequestFunctionBlockNumber")]
            [CommandOption("-n|--function-block-number|--number")]
            [DefaultValue((byte)0)]
            public byte FunctionBlockNumber { get; set; }

            [LocalizedDescription("ParameterRequestFunctionBlockInfoNotification")]
            [CommandOption("-i|--request-info")]
            [DefaultValue(true)]
            public bool RequestInfoNotification { get; set; }

            [LocalizedDescription("ParameterRequestFunctionBlockNameNotification")]
            [CommandOption("-f|--request-name")]
            [DefaultValue(true)]
            public bool RequestNameNotification { get; set; }

        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            // TODO: Validate at least one of the options is true 

            if (settings.RequestNameNotification == false && settings.RequestInfoNotification == false)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.FunctionBlockRequestInvalid));
                return ValidationResult.Error();
            }

            if (settings.FunctionBlockNumber > 31)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.FunctionBlockNumberInvalid));
                return ValidationResult.Error();
            }

            return ValidationResult.Success();
        }


        // TODO: Tons of strings in here that need to be localized

        public override int Execute(CommandContext context, Settings settings)
        {
            // build the message

            UInt64 timestamp = 0;   // send immediately

            MidiFunctionBlockDiscoveryRequests flags = MidiFunctionBlockDiscoveryRequests.None;

            if (settings.RequestInfoNotification) flags |= MidiFunctionBlockDiscoveryRequests.RequestFunctionBlockInfo;
            if (settings.RequestNameNotification) flags |= MidiFunctionBlockDiscoveryRequests.RequestFunctionBlockName;

            byte number = 0;

            if (settings.RequestAll)
            {
                number = 0xFF;  // request all function blocks
            }
            else
            {
                number = settings.FunctionBlockNumber;
            }

            var message = MidiStreamMessageBuilder.BuildFunctionBlockDiscoveryMessage(timestamp, number, flags);

            return (int)EndpointMessageSender.OpenTemporaryConnectionAndSendMidiMessage(settings.EndpointDeviceId, message);
        }
    }
}