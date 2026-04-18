// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using Microsoft.Windows.Devices.Midi2.Endpoints.Virtual;
using Microsoft.Windows.Devices.Midi2.Messages;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi;

namespace Microsoft.Midi.ConsoleApp
{
    internal class BridgeBleCommand : Command<BridgeBleCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBridgeEndpointsWinRTBleInputPortId")]
            [CommandOption("-i|--input|--winrt-input-ble-port")]
            public string WinRTInputPortId { get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsWinRTBleOutputPortId")]
            [CommandOption("-o|--output|--winrt-output-ble-port")]
            public string WinRTOutputPortId { get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsNewEndpointName")]
            [CommandOption("-n|--name|--new-endpoint-name")]
            public string NewEndpointName { get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsQuiet")]
            [CommandOption("-q|--quiet")]
            [DefaultValue(false)]
            public bool Quiet { get; set; }


            // name of loopback endpoint to create (with in and out groups)


            // --quiet if we shouldn't show traffic

        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.WinRTInputPortId) || string.IsNullOrWhiteSpace(settings.WinRTOutputPortId))
            {
                return ValidationResult.Error(Strings.CommandBridgeEndpointsValidationError_WinRTPortIdsMissing);
            }

            // validate these are bluetooth ports by looking for the magic string
            if (!settings.WinRTInputPortId.ToLower().Contains(".ble10") || !settings.WinRTOutputPortId.ToLower().Contains(".ble10"))
            {
                return ValidationResult.Error(Strings.CommandBridgeEndpointsValidationError_WinRTPortMustBeBluetooth);
            }

            // we don't check for presence here, because the app can wait for availability with a watcher
            if (string.IsNullOrWhiteSpace(settings.NewEndpointName))
            {
                return ValidationResult.Error(Strings.CommandBridgeEndpointsValidationError_NewEndpointNameMissing);
            }


            return ValidationResult.Success();
        }

        private MidiGroup _endpointGroup;

        private MidiVirtualDevice? CreateVirtualDevice(string endpointName, string productInstanceId)
        {
            _endpointGroup = new MidiGroup((byte)0);

            var endpointInfo = new MidiDeclaredEndpointInfo();
            endpointInfo.HasStaticFunctionBlocks = true;
            endpointInfo.Name = endpointName;
            endpointInfo.ProductInstanceId = productInstanceId;
            endpointInfo.SupportsMidi10Protocol = true;
            endpointInfo.SupportsMidi20Protocol = false;
            endpointInfo.SupportsReceivingJitterReductionTimestamps = false;
            endpointInfo.SupportsSendingJitterReductionTimestamps = false;
            endpointInfo.SpecificationVersionMajor = 1;
            endpointInfo.SpecificationVersionMinor = 1;

            var creationConfig = new MidiVirtualDeviceCreationConfig(
                endpointInfo.Name,
                Strings.CommandBridgeEndpoints_VirtualDeviceDescription,
                "Microsoft Corporation",
                endpointInfo);

            var block = new MidiFunctionBlock();
            block.Number = 0;
            block.IsActive = true;
            block.Name = "BLE MIDI IO";             // do not localize
            block.FirstGroup = _endpointGroup;
            block.GroupCount = 1;
            block.Direction = MidiFunctionBlockDirection.Bidirectional; // both an input and an output
            creationConfig.FunctionBlocks.Append(block);

            var virtualDevice = MidiVirtualDeviceManager.CreateVirtualDevice(creationConfig);

            if (virtualDevice != null)
            {
                // TODO log
                return virtualDevice;
            }
            else
            {
                // failed
                // TODO log
                return null;
            }

        }

        MidiInPort? _winRTMidiIn;
        IMidiOutPort? _winRTMidiOut;


        private async void OpenWinRTPorts(DeviceInformation inputDevice, DeviceInformation outputDevice)
        {
            try
            {
                Console.WriteLine($"Opening WinRT MIDI input '{inputDevice.Name}'");
                _winRTMidiIn = await MidiInPort.FromIdAsync(inputDevice.Id);
                if (_winRTMidiIn == null)
                {
                    // TODO: Log and display
                    AnsiConsole.Markup(AnsiMarkupFormatter.FormatError("Unable to open WinRT MIDI Input"));
                }

                Console.WriteLine($"Opening WinRT MIDI output '{outputDevice.Name}'");
                _winRTMidiOut = await MidiOutPort.FromIdAsync(outputDevice.Id);
                if (_winRTMidiOut == null)
                {
                    // TODO: Log and display
                    AnsiConsole.Markup(AnsiMarkupFormatter.FormatError("Unable to open WinRT MIDI Output"));
                }
            }
            catch (Exception ex)
            {
                AnsiConsole.Markup(AnsiMarkupFormatter.FormatError("Unable to open WinRT MIDI ports. " + ex.ToString()));

            }
        }



        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            // validate the BLE endpoint is available. If not, set a watcher on it

            // remember, we need to open both MIDI 1.0 in and MIDI 1.0 out


            // TODO: create a watcher for this specific endpoints if they aren't available

            DeviceInformation inputDevice;
            DeviceInformation outputDevice;

            try
            {

                AnsiConsole.WriteLine($"Checking Input Device '{settings.WinRTInputPortId}'");
                inputDevice = DeviceInformation.CreateFromIdAsync(settings.WinRTInputPortId).GetAwaiter().GetResult();

                AnsiConsole.WriteLine($"Checking Output Device '{settings.WinRTOutputPortId}'");
                outputDevice = DeviceInformation.CreateFromIdAsync(settings.WinRTOutputPortId).GetAwaiter().GetResult();

                if (inputDevice == null || outputDevice == null)
                {
                    // TODO: Log and display
                    AnsiConsole.Markup(AnsiMarkupFormatter.FormatError("Unable to open WinRT device information."));
                    return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
                }

                OpenWinRTPorts(inputDevice, outputDevice);

                if (_winRTMidiIn == null || _winRTMidiOut == null)
                {
                    AnsiConsole.Markup(AnsiMarkupFormatter.FormatError("Unable to open WinRT ports. Ports are null."));

                    return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
                }

            }
            catch (Exception ex)
            {
                // TODO: Log and display
                AnsiConsole.Markup(AnsiMarkupFormatter.FormatError("Unable to open WinRT device. " + ex.Message));
                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }




            Console.WriteLine("Creating virtual device...");
            string bleDeviceName = settings.NewEndpointName;
            string productInstanceId = "19770525";    // todo: we should generate some deterministic PID unique to the BLE device
            string sessionName = $"{Strings.CommandBridgeEndpoints_SessionNamePrefix}: {settings.NewEndpointName}";

            Console.Title = sessionName;


            var session = MidiSession.Create(sessionName);
            if (session == null)
            {
                // TODO: Log and output
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }

            var virtualDevice = CreateVirtualDevice(bleDeviceName, productInstanceId);
            if (virtualDevice == null)
            {
                session.Dispose();

                // TODO: Log and output
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var virtualConnection = session.CreateEndpointConnection(virtualDevice.DeviceEndpointDeviceId);
            if (virtualConnection == null)
            {
                session.Dispose();

                // TODO: Log and output
                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }

            virtualConnection.AddMessageProcessingPlugin(virtualDevice);

            // set up receive handler
            virtualConnection.MessageReceived += (sender, args) =>
            {
                // TODO get incoming message, convert it, and send out

                UInt32[] words = new UInt32[4];

                args.FillWordArray(0, words);

            };


            _winRTMidiIn.MessageReceived += (sender, args) =>
            {
                if (virtualConnection.IsOpen)
                {
                    var words = MidiMessageConverter.ConvertMidi1MessageToUmpWords(_endpointGroup, args.Message);

                    if (!settings.Quiet)
                    {
                        AnsiConsole.MarkupLine("Forwarding: " + AnsiMarkupFormatter.FormatMidiWords(words.ToArray()));
                    }

                    // forward the data
                    var results = virtualConnection.SendMultipleMessagesWordList(MidiClock.TimestampConstantSendImmediately, words);

                    if (MidiEndpointConnection.SendMessageSucceeded(results))
                    {
                        // all good

                        // todo: display words. Must account for there being more than a single message here

                     //   AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatMidiWords(words.ToArray()));
                    }
                    else
                    {
                        // todo: log and display
                    }
                }
            };

            Console.WriteLine("Opening virtual device...");

            virtualConnection.Open();









            bool continueWaiting = true;



            // Main UI loop ------------------------------------------------------------------------------------
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(Strings.BridgePressEscapeToStopMessage);
            AnsiConsole.WriteLine();

            // wait for escape

            while (continueWaiting)
            {
                if (Console.KeyAvailable)
                {
                    var keyInfo = Console.ReadKey(true);

                    if (keyInfo.Key == ConsoleKey.Escape)
                    {
                        continueWaiting = false;

                        // wake up the threads so they terminate
                        //m_terminateMessageListenerThread.Set();
                        //m_fileMessageThreadWakeup.Set();
                        //m_displayMessageThreadWakeup.Set();
                        //m_messageDispatcherThreadWakeup.Set();

                        AnsiConsole.WriteLine();
                        AnsiConsole.MarkupLine("🛑 " + Strings.BridgeEscapePressedMessage);
                    }

                }

                //if (_hasEndpointDisconnected)
                //{
                //    continueWaiting = false;
                //    //m_terminateMessageListenerThread.Set();
                //    //m_displayMessageThreadWakeup.Set();
                //    //m_fileMessageThreadWakeup.Set();
                //    //m_messageDispatcherThreadWakeup.Set();

                //    AnsiConsole.MarkupLine("❎ " + AnsiMarkupFormatter.FormatError(Strings.EndpointDisconnected));
                //}

                if (continueWaiting) Thread.Sleep(100);
            }



            session.Dispose();


            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
