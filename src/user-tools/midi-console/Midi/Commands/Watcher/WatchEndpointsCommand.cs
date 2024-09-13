// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Initialization;

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
            if (!MidiServicesInitializer.EnsureServiceAvailable())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(Strings.ErrorMidiServiceNotAvailable));
                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }

            MidiEndpointDeviceInformationFilters filter =
                MidiEndpointDeviceInformationFilters.StandardNativeUniversalMidiPacketFormat |
                MidiEndpointDeviceInformationFilters.StandardNativeMidi1ByteFormat |
                MidiEndpointDeviceInformationFilters.VirtualDeviceResponder;

            if (settings.IncludeDiagnosticLoopback)
            {
                filter |= MidiEndpointDeviceInformationFilters.DiagnosticLoopback;
            }

            _watcher = MidiEndpointDeviceWatcher.Create(filter);

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
            AnsiConsole.MarkupLine($"[darkorange]{Strings.NotificationEndpointDeviceWatcherStopped}[/]");
            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherEnumerationCompleted(MidiEndpointDeviceWatcher sender, object args)
        {
            AnsiConsole.MarkupLine($"[green]{Strings.NotificationEndpointDeviceWatcherInitialEnumerationCompleted}[/]");
            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherDeviceRemoved(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
        {
            AnsiConsole.MarkupLine($"[indianred1]{Strings.NotificationEndpointDeviceWatcherEndpointRemoved}[/]");
            AnsiConsole.MarkupLine(" - " + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.EndpointDeviceId));
            AnsiConsole.MarkupLine("");
        }

        private const string _deviceBullet = " 🎹 ";
        private const string _bullet = "[grey] ➡️ [/]";
        private const string _emptyBullet = "   ";

        private void OnWatcherDeviceUpdated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdatedEventArgs args)
        {
            var di = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(args.EndpointDeviceId);

            AnsiConsole.MarkupLine($"[steelblue1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdated}[/]");
            AnsiConsole.MarkupLine(_emptyBullet + AnsiMarkupFormatter.FormatEndpointName(di.Name));
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(di.EndpointDeviceId));

            
            if (args.IsNameUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedName}[/]");
            }

            if (args.IsEndpointInformationUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedEndpointInformation}[/]");
            }

            if (args.IsStreamConfigurationUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedStreamConfiguration}[/]");
            }

            if (args.AreFunctionBlocksUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedFunctionBlocks}[/]");
            }

            if (args.IsDeviceIdentityUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedDeviceIdentity}[/]");
            }

            if (args.IsUserMetadataUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedUserMetadata}[/]");
            }

            if (args.AreAdditionalCapabilitiesUpdated)
            {
                AnsiConsole.MarkupLine(_bullet + $"[gold3_1]{Strings.NotificationEndpointDeviceWatcherEndpointUpdatedAdditionalCapabilities}[/]");
            }

            AnsiConsole.MarkupLine("");
        }

        private void OnWatcherDeviceAdded(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
        {
            AnsiConsole.MarkupLine(Strings.NotificationEndpointDeviceWatcherEndpointAdded);
            AnsiConsole.MarkupLine(_deviceBullet + AnsiMarkupFormatter.FormatFullEndpointInterfaceId(args.AddedDevice.EndpointDeviceId));
            AnsiConsole.MarkupLine(_emptyBullet + AnsiMarkupFormatter.FormatEndpointName(args.AddedDevice.Name));
            AnsiConsole.MarkupLine(_emptyBullet + args.AddedDevice.EndpointPurpose.ToString());
            AnsiConsole.MarkupLine("");
        }


    }
}
