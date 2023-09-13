using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Midi2;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class SendMessagesFileCommand : Command<SendMessagesFileCommand.Settings>
    {
        public sealed class Settings : SendMessageCommandSettings
        {
            [LocalizedDescription("ParameterSendMessagesFileCommandFile")]
            [CommandArgument(2, "<Input File>")]
            public string InputFile { get; set; }

            [LocalizedDescription("ParameterSendMessagesFileVerbose")]
            [CommandOption("-v|--verbose|--details")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            // we won't overwrite existing files. Intent is to prevent misuse of this app
            if (!System.IO.File.Exists(settings.InputFile))
            {
                return ValidationResult.Error($"File not found {settings.InputFile}.");
            }

            return base.Validate(context, settings);
        }

        private bool ValidateMessage(UInt32[] words)
        {
            if (words.Length > 0 && words.Length <= 4)
            {
                // allowed behavior is to cast the packet type to the word count
                return (bool)((int)MidiUmpUtility.GetPacketTypeFromFirstUmpWord(words[0]) == words.Length);
            }
            else
            {
                return false;
            }
        }

        private void SendSingleMessage(IMidiOutputConnection endpoint, UInt32[] words)
        {

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.InstanceId))
            {
                endpointId = settings.InstanceId.Trim().ToLower();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickOutput();
            }

            // TODO





            return (int)MidiConsoleReturnCode.Success;
        }









    }
}
