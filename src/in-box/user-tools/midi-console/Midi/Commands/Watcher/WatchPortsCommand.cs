// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    internal class WatchPortsCommand : Command<WatchPortsCommand.Settings>
    {
        private MidiLegacyPortDeviceWatcher? _watcher = null;
        internal class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterEnumLegacyPortsVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }

        private Settings _settings;

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            _settings = settings;

            _watcher = MidiLegacyPortDeviceWatcher.Create();

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

        private void OnWatcherStopped(MidiLegacyPortDeviceWatcher sender, object args)
        {
            AnsiConsole.MarkupLine($"[darkorange]{Strings.NotificationLegacyPortDeviceWatcherStopped}[/]");
            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherEnumerationCompleted(MidiLegacyPortDeviceWatcher sender, object args)
        {
            AnsiConsole.MarkupLine($"[green]{Strings.NotificationLegacyPortDeviceWatcherInitialEnumerationCompleted}[/]");
            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherDeviceRemoved(MidiLegacyPortDeviceWatcher sender, MidiLegacyPortDeviceInformationRemovedEventArgs args)
        {
            AnsiConsole.Markup($"[indianred1]{Strings.NotificationLegacyPortDeviceWatcherPortRemoved}[/]");
            AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.RemovedDevice.PortDeviceId));

            if (args.RemovedDevice.Flow == Midi1PortFlow.MidiMessageSource)
            {
                AnsiConsole.MarkupLine(_emptyBullet + Strings.DirectionMessageSource);
            }
            else
            {
                AnsiConsole.MarkupLine(_emptyBullet + Strings.DirectionMessageDestination);
            }

            AnsiConsole.MarkupLine("");
        }

        private const string _deviceBullet = " 🎹 ";
        private const string _bullet = "[grey] ➡️ [/]";
        private const string _emptyBullet = "   ";

        private void OnWatcherDeviceUpdated(MidiLegacyPortDeviceWatcher sender, MidiLegacyPortDeviceInformationUpdatedEventArgs args)
        {
            AnsiConsole.Markup($"[steelblue1]{Strings.NotificationLegacyPortDeviceWatcherPortUpdated}[/]");
            AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.UpdatedDevice.PortDeviceId));
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatEndpointName(args.UpdatedDevice.Name));

            if (args.UpdatedDevice.Flow == Midi1PortFlow.MidiMessageSource)
            {
                AnsiConsole.MarkupLine(_emptyBullet + Strings.DirectionMessageSource);
            }
            else
            {
                AnsiConsole.MarkupLine(_emptyBullet + Strings.DirectionMessageDestination);
            }

            if (args.IsNameUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[green]{Strings.NotificationLegacyPortDeviceWatcherEndpointUpdatedName}[/]");
            }

            if (args.IsNumberUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[green]{Strings.NotificationLegacyPortDeviceWatcherEndpointUpdatedNumber}[/]");
                AnsiConsole.MarkupLine(_bullet + "Port Number: " + AnsiMarkupFormatter.FormatGeneralNumber(args.UpdatedDevice.Number));
            }



            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherDeviceAdded(MidiLegacyPortDeviceWatcher sender, MidiLegacyPortDeviceInformationAddedEventArgs args)
        {
            AnsiConsole.Markup($"[CadetBlue]{Strings.NotificationLegacyPortDeviceWatcherPortAdded}[/]");
            AnsiConsole.MarkupLine(" " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.AddedDevice.PortDeviceId));

            AnsiConsole.MarkupLine(_emptyBullet + AnsiMarkupFormatter.FormatEndpointName(args.AddedDevice.Name));
            AnsiConsole.MarkupLine(_emptyBullet + "Port Number: " + AnsiMarkupFormatter.FormatGeneralNumber(args.AddedDevice.Number));

            if (args.AddedDevice.Flow == Midi1PortFlow.MidiMessageSource)
            {
                AnsiConsole.MarkupLine(_emptyBullet + Strings.DirectionMessageSource);
            }
            else
            {
                AnsiConsole.MarkupLine(_emptyBullet + Strings.DirectionMessageDestination);
            }
            AnsiConsole.MarkupLine("");
        }


    }
}
