using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

//using Microsoft.Windows.Devices.Midi2;

namespace Microsoft.Midi.ConsoleApp
{

    internal class EndpointSendSysExFileCommand : Command<EndpointSendSysExFileCommand.Settings>
    {
        public sealed class Settings : EndpointCommandSettings
        {
            [LocalizedDescription("ParameterSendSysExFileCommandFile")]
            [CommandArgument(1, "<Input File>")]
            public string? InputFile { get; set; }

            [LocalizedDescription("ParameterSendSysExFileVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }

            //[EnumLocalizedDescription("ParameterSysExFileFormat", typeof(MidiSystemExclusiveDataReaderFormat))]
            //[CommandOption("-f|--sysex-file-type")]
            //[DefaultValue(MidiSystemExclusiveDataReaderFormat.InferFromData)]
            //public MidiSystemExclusiveDataReaderFormat FileType { get; set; }

            //[EnumLocalizedDescription("ParameterSysExDataFormat", typeof(MidiSystemExclusiveDataFormat))]
            //[CommandOption("-d|--source-data-format")]
            //[DefaultValue(MidiSystemExclusiveDataFormat.InferFromData)]
            //public MidiSystemExclusiveDataFormat SourceDataFormat { get; set; }

            [LocalizedDescription("ParameterSendSysExFileReplaceGroup")]
            [CommandOption("-g|--new-group-index")]
            public int? NewGroupIndex { get; set; }


            [LocalizedDescription("ParameterSendMessageDelayBetweenMessages")]
            [CommandOption("-p|--pause|--delay")]
            [DefaultValue(2)]
            public int DelayBetweenMessages { get; set; }

        }


        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.InputFile == null)
            {
                // TODO: Localize
                return ValidationResult.Error($"File not specified.");
            }

            if (settings.InputFile != null && !System.IO.File.Exists(settings.InputFile))
            {
                // TODO: Localize
                return ValidationResult.Error($"File not found {settings.InputFile}.");
            }

            if (settings.NewGroupIndex.HasValue)
            {
                byte newGroup = (byte)settings.NewGroupIndex.GetValueOrDefault(0);

                if (!MidiGroup.IsValidIndex(newGroup))
                {
                    return ValidationResult.Error(Strings.ValidationErrorInvalidGroup);
                }
            }

            return base.Validate(context, settings);
        }


        public override int Execute(CommandContext context, Settings settings)
        {
            if (!MidiServicesInitializer.EnsureServiceAvailable())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));

                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }


            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.EndpointDeviceId))
            {
                endpointId = settings.EndpointDeviceId.Trim();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickEndpoint();
            }

            if (!string.IsNullOrEmpty(endpointId))
            {
                // TODO: Update loc strings
                string endpointName = EndpointUtility.GetEndpointNameFromEndpointInterfaceId(endpointId);

                AnsiConsole.Markup(Strings.SendMessageSendingThroughEndpointLabel);
                AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatEndpointName(endpointName));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointId));


                using var session = MidiSession.Create($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");
                if (session == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                    return (int)MidiConsoleReturnCode.ErrorCreatingSession;
                }

                var connection = session.CreateEndpointConnection(endpointId);
                if (connection == null)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));

                    return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
                }

                bool openSuccess = openSuccess = connection.Open(); ;
                if (!openSuccess)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));

                    return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }


                // TODO: get a reader for the file and send it to the sysex SDK helper




                return (int)MidiConsoleReturnCode.Success;

            }
            else
            {
                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }

        }






    }
}
