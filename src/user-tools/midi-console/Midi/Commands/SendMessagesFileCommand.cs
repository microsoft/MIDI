using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class SendMessagesFileCommand : Command<SendMessagesFileCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterCommonInstanceIdDescription")]
            [CommandOption("-i|--instance-id")]
            public string InstanceId { get; init; }


            [LocalizedDescription("ParameterSendMessagesFileCommandFile")]
            [CommandOption("-f|--file")]
            public string File { get; init; }

            [LocalizedDescription("ParameterSendMessageDelayBetweenMessages")]
            [CommandOption("-p|--pause|--delay")]
            [DefaultValue(100)]
            public int DelayBetweenMessages { get; init; }

        }

        public override int Execute(CommandContext context, Settings settings)
        {

            return 0;
        }
    }
}
