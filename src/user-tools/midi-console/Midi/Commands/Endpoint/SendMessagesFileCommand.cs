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

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class SendMessagesFileCommand : Command<SendMessagesFileCommand.Settings>
    {
        public sealed class Settings : SendMessageCommandSettings
        {
            [LocalizedDescription("ParameterSendMessagesFileCommandFile")]
            [CommandArgument(2, "<Input File>")]
            public string InputFile { get; set; }

            [EnumLocalizedDescription("ParameterSendMessagesFileFieldDelimiter", typeof(ParseFieldDelimiter))]
            [CommandOption("-d|--delimiter")]
            [DefaultValue(ParseFieldDelimiter.Auto)]
            public ParseFieldDelimiter FieldDelimiter { get; set; }

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
            if (words != null && words.Length > 0 && words.Length <= 4)
            {
                // allowed behavior is to cast the packet type to the word count
                return (bool)((int)MidiUmpUtility.GetPacketTypeFromFirstUmpWord(words[0]) == words.Length);
            }
            else
            {
                return false;
            }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            MidiSession? session = null;
            IMidiOutputConnection? connection = null;

            string endpointId = string.Empty;

            if (!string.IsNullOrEmpty(settings.InstanceId))
            {
                endpointId = settings.InstanceId.Trim();
            }
            else
            {
                endpointId = UmpEndpointPicker.PickOutput();
            }

            // TODO: Update loc strings
            AnsiConsole.MarkupLine(Strings.SendMessageSendingThroughEndpointLabel + ": " + AnsiMarkupFormatter.FormatDeviceInstanceId(endpointId));
            AnsiConsole.WriteLine();

            bool openSuccess = false;

            AnsiConsole.Status()
                .Start(Strings.StatusCreatingSessionAndOpeningEndpoint, ctx =>
                {
                    ctx.Spinner(Spinner.Known.Star);

                    // todo: update loc strings
                    session = MidiSession.CreateSession($"{Strings.AppShortName} - {Strings.SendMessageSessionNameSuffix}");


                    if (session != null)
                    {
                        var endpointDirection = EndpointUtility.GetUmpEndpointTypeFromInstanceId(endpointId);

                        if (endpointDirection == EndpointDirection.Bidirectional)
                        {
                            connection = session.ConnectBidirectionalEndpoint(endpointId);
                        }
                        else if (endpointDirection == EndpointDirection.Out)
                        {
                            connection = session.ConnectOutputEndpoint(endpointId);
                        }
                    }

                    if (connection != null)
                    {
                        openSuccess = false;

                        if (connection is MidiBidirectionalEndpointConnection)
                        {
                            openSuccess = ((MidiBidirectionalEndpointConnection)(connection)).Open();
                        }
                        else if (connection is MidiOutputEndpointConnection)
                        {
                            openSuccess = ((MidiOutputEndpointConnection)(connection)).Open();
                        }

                    }
                });

            if (session == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateSession));
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }
            else if (connection == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToCreateEndpointConnection));

                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }
            else if (!openSuccess)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorUnableToOpenEndpoint));

                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }

            var table = new Table();

            // if not verbose, just show a status spinner


            // if verbose, spin up a live table

            AnsiConsole.Live(table)
                .Start(ctx =>
                {
                    table.AddColumn("Line");               
                    table.AddColumn("Timestamp");          // a file with a timestamp isn't useful, really. But we could support an offset like +value
                    table.AddColumn("Sent Data");
                    table.AddColumn("Message Type");

                    ctx.Refresh();
                    //AnsiConsole.WriteLine("Created table");

                    // get starting timestamp for any offset
                    var startingTimestamp = MidiClock.GetMidiTimestamp();

                    // open our data file
                    var fileStream = System.IO.File.OpenText(settings.InputFile);

                    char delimiter = (char)0;

                    switch (settings.FieldDelimiter)
                    {
                        case ParseFieldDelimiter.Auto:
                            // we'll evaluate on each line
                            break;
                        case ParseFieldDelimiter.Space:
                            delimiter = ' ';
                            break;
                        case ParseFieldDelimiter.Comma:
                            delimiter = ',';
                            break;
                        case ParseFieldDelimiter.Pipe:
                            delimiter = '|';
                            break;
                        case ParseFieldDelimiter.Tab:
                            delimiter = '\t';
                            break;
                    }

                    if (fileStream != null)
                    {
                        uint lineNumber = 0;        // if someone has a file with more than uint.MaxValue / 4.3 billion lines, we'll overflow :)

                        string? line = string.Empty;

                        while (!fileStream.EndOfStream && line != null)
                        {
                            line = fileStream.ReadLine();

                            if (line == null)
                                continue;

                            lineNumber++;

                            // skip over comments and white space
                            if (LineIsIgnorable(line))
                                continue;

                            // if we're using Auto for the delimiter, each line is evaluated in case the file is mixed
                            // if you don't want to take this hit, specify the delimiter on the command line
                            if (settings.FieldDelimiter == ParseFieldDelimiter.Auto)
                                delimiter = IdentifyFieldDelimiter(line);

                            UInt32[] words;

                            // ignore files with timestamps for this first version

                            if (ParseNextDataLine(line, delimiter, (int)settings.WordDataFormat, out words))
                            {
                                if (ValidateMessage(words))
                                {
                                    var timestamp = MidiClock.GetMidiTimestamp();

                                    connection.SendUmpWordArray(timestamp, words, (UInt32)words.Count());

                                    table.AddRow(
                                        AnsiMarkupFormatter.FormatGeneralNumber(lineNumber), 
                                        AnsiMarkupFormatter.FormatTimestamp(timestamp),
                                        AnsiMarkupFormatter.FormatMidiWords(words),
                                        AnsiMarkupFormatter.FormatMessageType(MidiUmpUtility.GetMessageTypeFromFirstUmpWord(words[0]))
                                        );

                                    ctx.Refresh();
                                    Thread.Sleep(settings.DelayBetweenMessages);
                                }
                                else
                                {
                                    // invalid UMP
                                    table.AddRow(
                                        AnsiMarkupFormatter.FormatGeneralNumber(lineNumber), 
                                        "", 
                                        AnsiMarkupFormatter.FormatError("Line does not contain a valid UMP") + "\n\"" + line + "\"",
                                        ""
                                        );

                                    ctx.Refresh();
                                    Thread.Sleep(0);
                                }
                            }
                            else
                            {
                                // report line number and that it is an error
                                table.AddRow(
                                    AnsiMarkupFormatter.FormatGeneralNumber(lineNumber), 
                                    "", 
                                    AnsiMarkupFormatter.FormatError("Unable to parse MIDI words from line") + "\n\"" + line + "\"",
                                    ""
                                    );

                                ctx.Refresh();
                                Thread.Sleep(0);
                            }
                        }
                    }
                    else
                    {
                        // file stream is null
                        AnsiConsole.WriteLine(AnsiMarkupFormatter.FormatError("Unable to open file. File stream is null"));
                    }
                });




            return (int)MidiConsoleReturnCode.Success;
        }

        // true if the line is a comment or is white space
        private bool LineIsIgnorable(string inputLine)
        {
            // empty line
            if (string.IsNullOrEmpty(inputLine))
                return true;

            // comment
            if (inputLine.Trim().StartsWith("#"))
                return true;

            return false;
        }

        private char IdentifyFieldDelimiter(string line)
        {
            line = line.Trim();

            if (line.Contains(","))
                return ',';

            if (line.Contains("|"))
                return '|';

            if (line.Contains("\t"))
                return '\t';

            // some people use ", " between words instead of just ",". We need to be
            // sure to allow for that and related so we check this last
            if (line.Contains(" "))
                return ' ';

            return (char)0;
        }

        // returns true if it could parse the line, false if not.
        //private bool ParseNextDataLine(string inputLine, char delimiter, out bool timestampIsOffset, out UInt64 timestamp, out UInt32[] words)
        //{

        //}

        private bool ParseNextDataLine(string inputLine, char delimiter, int fromBase, out UInt32[] words)
        {
            try
            {
                // in the case of Auto, no delimiter can be found if the line just has one entry
                if (delimiter == (char)0)
                {
                    words = new UInt32[1];

                    words[0] = ParseMidiWord(inputLine.Trim(), fromBase); 

                    return true;
                }
                else
                {
                    var strings = inputLine.Split(delimiter);

                    if (strings == null)
                    {
                        words = null;
                        return false;
                    }

                    words = new UInt32[strings.Length];

                    for (int i = 0; i < strings.Length; i++)
                    {
                        // TODO: Use the word data format to convert to base 10

                        words[i] = ParseMidiWord(strings[i].Trim(), fromBase);
                    }

                    return true;
                }
            }
            catch (Exception ex) 
            {
                //AnsiConsole.WriteException(ex);

                words = null;
                return false;
            }
        }

        private UInt32 ParseMidiWord(string midiWord, int fromBase)
        {
            // support an "h" suffix for hex
            if (fromBase == 16 && midiWord.ToLower().EndsWith("h"))
            {
                midiWord = midiWord.Substring(0, midiWord.Length - 1);
            }
            else if (fromBase == 10 && midiWord.ToLower().EndsWith("d"))
            {
                midiWord = midiWord.Substring(0, midiWord.Length - 1);
            }
            else if (fromBase == 2 && midiWord.ToLower().EndsWith("b"))
            {
                midiWord = midiWord.Substring(0, midiWord.Length - 1);
            }


            return Convert.ToUInt32(midiWord, fromBase);
        }



    }
}
