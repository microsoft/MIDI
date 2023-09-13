using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    // might be good if this could also be an index into a list we display
    // could make it optional, and then if missing, use AnsiConsole.Prompt()
    internal class EndpointCommandSettings : CommandSettings
    {
        [LocalizedDescription("ParameterCommonInstanceIdDescription")]
        [CommandArgument(0, "[Instance Id]")]
        public string InstanceId { get; set; }
    }

    internal class SendMessageCommandSettings : EndpointCommandSettings
    {
        [LocalizedDescription("ParameterSendMessageDelayBetweenMessages")]
        [CommandOption("-p|--pause|--delay")]
        [DefaultValue(10)]
        public int DelayBetweenMessages { get; set; }

        [EnumLocalizedDescription("ParameterSendMessageWordFormat", typeof(MidiWordDataFormat))]
        [CommandOption("-w|--word-format")]
        [DefaultValue(MidiWordDataFormat.Hex)]
        public MidiWordDataFormat WordDataFormat { get; set; }


    }


}
