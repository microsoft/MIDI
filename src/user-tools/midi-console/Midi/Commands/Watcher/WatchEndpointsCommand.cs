// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
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
            if (!MidiService.EnsureServiceAvailable())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MIDI Service is not available."));
                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }


            MidiEndpointDeviceInformationFilters filter =
                MidiEndpointDeviceInformationFilters.IncludeClientUmpNative |
                MidiEndpointDeviceInformationFilters.IncludeClientByteStreamNative |
                MidiEndpointDeviceInformationFilters.IncludeVirtualDeviceResponder;

            if (settings.IncludeDiagnosticLoopback)
            {
                filter |= MidiEndpointDeviceInformationFilters.IncludeDiagnosticLoopback;
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


        private void OnWatcherDeviceRemoved(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
        {
            AnsiConsole.MarkupLine("[indianred1]Endpoint Removed:[/]");
            AnsiConsole.MarkupLine(" - " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.EndpointDeviceId));
            AnsiConsole.MarkupLine("");

        }

        private const string _deviceBullet = " 🎹 ";
        private const string _bullet = "[grey] ➡️ [/]";
        private const string _emptyBullet = "   ";

        private void OnWatcherDeviceUpdated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdatedEventArgs args)
        {
            AnsiConsole.MarkupLine("[steelblue1]Endpoint Updated:[/]");
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.EndpointDeviceId));

            

            if (args.IsNameUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Name Updated[/]");
            }

            if (args.IsEndpointInformationUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Endpoint Information Updated[/]");
            }

            if (args.IsStreamConfigurationUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Stream Configuration Updated[/]");
            }

            if (args.AreFunctionBlocksUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Function Blocks Updated[/]");
            }

            if (args.IsDeviceIdentityUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Device Identity Updated[/]");
            }

            if (args.IsUserMetadataUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]User Metadata Updated[/]");
            }

            if (args.AreAdditionalCapabilitiesUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + "[gold3_1]Additional Capabilities Updated[/]");
            }

            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherDeviceAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
        {
            AnsiConsole.MarkupLine("Endpoint Added: ");
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.AddedDevice.EndpointDeviceId));
            AnsiConsole.MarkupLine(_emptyBullet + AnsiMarkupFormatter.FormatEndpointName(args.AddedDevice.Name));
            AnsiConsole.MarkupLine(_emptyBullet + args.AddedDevice.EndpointPurpose.ToString());
            AnsiConsole.MarkupLine("");

        }
    }
}
