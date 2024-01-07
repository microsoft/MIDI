using Spectre.Console.Cli;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;
using Windows.Devices.Midi2;
using Windows.UI.Input.Inking.Analysis;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    internal class WatchEndpointsCommand : Command<WatchEndpointsCommand.Settings>
    {
        private MidiEndpointDeviceWatcher? _watcher = null;
        internal class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumEndpointsIncludeLoopbackEndpoints")]
            [CommandOption("-l|--include-loopback")]
            [DefaultValue(false)]
            public bool IncludeDiagnosticLoopback { get; set; }

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            MidiEndpointDeviceInformationFilter filter = 
                MidiEndpointDeviceInformationFilter.IncludeClientUmpNative | 
                MidiEndpointDeviceInformationFilter.IncludeClientByteStreamNative | 
                MidiEndpointDeviceInformationFilter.IncludeVirtualDeviceResponder;

            if (settings.IncludeDiagnosticLoopback)
            {
                filter |= MidiEndpointDeviceInformationFilter.IncludeDiagnosticLoopback;
            }

            _watcher = MidiEndpointDeviceWatcher.CreateWatcher(filter);


            _watcher.Added += OnWatcherDeviceAdded;
            _watcher.Removed += OnWatcherDeviceRemoved;
            _watcher.Updated += OnWatcherDeviceUpdated;

            _watcher.EnumerationCompleted += OnWatcherEnumerationCompleted;
            _watcher.Stopped += OnWatcherStopped;

            AnsiConsole.MarkupLine(Strings.WatcherPressEscapeToStopWatchingMessage);
            AnsiConsole.WriteLine();

            _watcher.Start();


            bool continueWaiting = true;

            while (continueWaiting)
            {
                if (Console.KeyAvailable)
                {
                    var keyInfo = Console.ReadKey(true);

                    if (keyInfo.Key == ConsoleKey.Escape)
                    {
                        continueWaiting = false;

                        AnsiConsole.MarkupLine(Strings.WatcherEscapePressedMessage);
                        break;
                    }
                }

                Thread.Sleep(0);
            }

            _watcher.Stop();


            _watcher.Added -= OnWatcherDeviceAdded;
            _watcher.Removed -= OnWatcherDeviceRemoved;
            _watcher.Updated -= OnWatcherDeviceUpdated;

            _watcher.EnumerationCompleted -= OnWatcherEnumerationCompleted;
            _watcher.Stopped -= OnWatcherStopped;


            return (int)MidiConsoleReturnCode.Success;
        }

        private void OnWatcherStopped(MidiEndpointDeviceWatcher sender, object args)
        {
            AnsiConsole.MarkupLine("[darkorange]Watcher Stopped[/]");
            AnsiConsole.MarkupLine("");

        }

        private void OnWatcherEnumerationCompleted(MidiEndpointDeviceWatcher sender, object args)
        {
            AnsiConsole.MarkupLine("[green]Enumeration Completed[/]");
            AnsiConsole.MarkupLine("");

        }


        private void OnWatcherDeviceRemoved(MidiEndpointDeviceWatcher sender, Windows.Devices.Enumeration.DeviceInformationUpdate args)
        {
            AnsiConsole.MarkupLine("[indianred1]Endpoint Removed:[/]");
            AnsiConsole.MarkupLine(" - " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.Id));
            AnsiConsole.MarkupLine("");

        }

        private const string _bullet = "[grey] ☑️ [/]";
        private const string _deviceBullet = " 🎹 ";

        private void OnWatcherDeviceUpdated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdateEventArgs args)
        {
            AnsiConsole.MarkupLine("[steelblue1]Endpoint Updated:[/]");
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.Id));

            

            if (args.UpdatedName)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Name Updated[/]");
            }

            if (args.UpdatedEndpointInformation)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Endpoint Information Updated[/]");
            }

            if (args.UpdatedStreamConfiguration)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Stream Configuration Updated[/]");
            }

            if (args.UpdatedFunctionBlocks)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Function Blocks Updated[/]");
            }

            if (args.UpdatedDeviceIdentity)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Device Identity Updated[/]");
            }

            if (args.UpdatedUserMetadata)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]User Metadata Updated[/]");
            }

            if (args.UpdatedAdditionalCapabilities)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Additional Capabilities Updated[/]");
            }

            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherDeviceAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformation args)
        {
            AnsiConsole.MarkupLine("Endpoint Added: ");
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.Id));
            AnsiConsole.MarkupLine(_bullet + AnsiMarkupFormatter.FormatEndpointName(args.Name));
            AnsiConsole.MarkupLine(_bullet + args.EndpointPurpose.ToString());
            AnsiConsole.MarkupLine("");

        }
    }
}
