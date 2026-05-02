// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using System.Runtime.InteropServices;
using Microsoft.Windows.Devices.Midi2.NetProjection;
using System.Data.Common;
using System.Net;
using System.Runtime.InteropServices;
using WinRT;

namespace Microsoft.Midi.ConsoleApp
{
    [ComVisible(true)]
    [Guid("33712d34-e3a3-403c-8ee2-587a10abb17e")]  // Unique GUID for the class
    [ClassInterface(ClassInterfaceType.None)]       // Use explicit interface implementation
    internal unsafe class EndpointConnectionCallback : IMidiEndpointConnectionMessagesReceivedCallback
    {
        private IMidiEndpointConnectionRaw? _connectionToForwardTo;
        private IMidiEndpointConnectionRaw? _myConnection;
        private IntPtr _thisCallbackObjInterfacePointer;
        //private GCHandle _thisCallbackGCHandle;

        public EndpointConnectionCallback()
        {
        }

        ~EndpointConnectionCallback()
        {
            // clean up the callback from the connection to avoid potential access after free issues
            if (_connectionToForwardTo != null)
            {
                _connectionToForwardTo = null;
            }

            if (_myConnection != null)
            {
                _myConnection.RemoveMessagesReceivedCallback();
                _myConnection = null;
            }

            if (_thisCallbackObjInterfacePointer != IntPtr.Zero)
            {
                Marshal.Release(_thisCallbackObjInterfacePointer);
                _thisCallbackObjInterfacePointer = IntPtr.Zero;
            }

            //if (_thisCallbackGCHandle.IsAllocated)
            //{
            //    _thisCallbackGCHandle.Free();
            //}

        }

        public bool SetThisConnection(MidiEndpointConnection myConnection)
        {
            AnsiConsole.WriteLine($"MidiEndpointConnectionBridge: SetThisConnection {myConnection.ConnectedEndpointDeviceId}.");

            _myConnection = myConnection.GetMidiEndpointConnectionRawInterface();

            var hr = Marshal.QueryInterface(
                Marshal.GetIUnknownForObject(this),
                typeof(IMidiEndpointConnectionMessagesReceivedCallback).GUID,
                out _thisCallbackObjInterfacePointer);

            if ((hr == 0) && _thisCallbackObjInterfacePointer == IntPtr.Zero)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Unable to get IMidiEndpointConnectionMessagesReceivedCallback interface"));
                return false;
            }

            _myConnection.SetMessagesReceivedCallback(_thisCallbackObjInterfacePointer);

            //_thisCallbackGCHandle = GCHandle.Alloc(this, GCHandleType.Pinned);

            AnsiConsole.WriteLine("MidiEndpointConnectionBridge: SetThisConnection Complete");

            return true;
        }

        public void SetConnectionToForwardTo(MidiEndpointConnection connectionToForwardTo)
        {
            AnsiConsole.WriteLine($"MidiEndpointConnectionBridge: SetConnectionToForwardTo {connectionToForwardTo.ConnectedEndpointDeviceId}.");

            _connectionToForwardTo = connectionToForwardTo.GetMidiEndpointConnectionRawInterface();

            AnsiConsole.WriteLine("MidiEndpointConnectionBridge: SetConnectionToForwardTo Complete");
        }


        public unsafe void MessagesReceived(Guid sessionId, Guid connectionId, ulong timestamp, uint wordCount, uint* messages)
        {
            if (messages == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MessagesReceived: messages pointer is null"));
                return;
            }

            AnsiConsole.WriteLine($"MessagesReceived: {timestamp} {wordCount} words received.");

            try
            {
                _connectionToForwardTo?.SendMidiMessagesRaw(timestamp, wordCount, messages);
            }
            catch (Exception ex)
            {
                AnsiConsole.WriteException(ex);
            }
        }
    }

    internal class MidiEndpointConnectionBridge
    {
        private EndpointConnectionCallback? _callbackA;
        private EndpointConnectionCallback? _callbackB;

        //private readonly IntPtr _callbackAPointer = IntPtr.Zero;
        //private readonly IntPtr _callbackBPointer = IntPtr.Zero;

        public MidiEndpointConnectionBridge(MidiEndpointConnection connectionA, MidiEndpointConnection connectionB)
        {
            if (connectionA == null || connectionB == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MidiEndpointConnectionBridge: Connection raw interfaces are null"));
                return;
            }

            _callbackA = new EndpointConnectionCallback();
            _callbackB = new EndpointConnectionCallback();

            if (_callbackA == null || _callbackB == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("MidiEndpointConnectionBridge: Callbacks are null"));
                return;
            }

            _callbackA.SetThisConnection(connectionA);
            _callbackA.SetConnectionToForwardTo(connectionB);

            _callbackB.SetThisConnection(connectionB);
            _callbackB.SetConnectionToForwardTo(connectionA);

            AnsiConsole.WriteLine("MidiEndpointConnectionBridge: Forwarding set. Ready to go");
        }


    }








    internal class BridgeEndpointsCommand : Command<BridgeEndpointsCommand.Settings>
    {
        internal class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBridgeEndpointsEndpointIdA")]
            [CommandOption("-a|--endpoint-id-a")]
            public string? EndpointIdA{ get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsEndpointIdB")]
            [CommandOption("-b|--endpoint-id-b")]
            public string? EndpointIdB { get; set; }

            [LocalizedDescription("ParameterBridgeEndpointsQuiet")]
            [CommandOption("-q|--quiet")]
            [DefaultValue(false)]
            public bool Quiet { get; set; }


            // TODO: This isn't going to be very useful unless we can provide a way to map the groups
            // from each side. LIke an array of A and B groups
            // or we can maybe provide just a way to map a single group to a single group and have
            // just the A and B side groups specified. That will work better for MIDI 1-style connections

            // consider if we should allow mapping to the same endpoint, but different groups. Think through
            // how that would work


        }


        public override Spectre.Console.ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (!string.IsNullOrWhiteSpace(settings.EndpointIdA))
            {
                if (!MidiEndpointDeviceIdHelper.IsWindowsMidiServicesEndpointDeviceId(settings.EndpointIdA))
                {
                    return Spectre.Console.ValidationResult.Error("EndpointIdA is not a valid Windows MIDI Services endpoint device id.");
                }
            }

            if (!string.IsNullOrWhiteSpace(settings.EndpointIdB))
            {
                if (!MidiEndpointDeviceIdHelper.IsWindowsMidiServicesEndpointDeviceId(settings.EndpointIdB))
                {
                    return Spectre.Console.ValidationResult.Error("EndpointIdB is not a valid Windows MIDI Services endpoint device id.");
                }
            }

            return ValidationResult.Success();
        }


        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            var endpointIdA = string.Empty;
            var endpointIdB = string.Empty;

            if (string.IsNullOrEmpty(settings.EndpointIdA))
            {
                endpointIdA = UmpEndpointPicker.PickEndpoint("Please select the first endpoint to bridge.");
            }
            else
            {
                endpointIdA = settings.EndpointIdA.Trim().ToLower();
            }

            if (string.IsNullOrEmpty(settings.EndpointIdB))
            {
                endpointIdB = UmpEndpointPicker.PickEndpoint("Please select the second endpoint to bridge. Currently, the groups will need to match.");
            }
            else
            {
                endpointIdB = settings.EndpointIdB.Trim().ToLower();
            }

            var endpointDeviceInformationA = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(endpointIdA);
            var endpointDeviceInformationB = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(endpointIdB);

            // ensure the endpoints exist
            if (endpointDeviceInformationA == null || endpointDeviceInformationB == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("One or both endpoints could not be found. Error looking up endpoint device information."));

                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }
            else
            {
                AnsiConsole.MarkupLine($"Endpoint A: {endpointDeviceInformationA.Name} ({endpointDeviceInformationA.EndpointDeviceId})");
                AnsiConsole.MarkupLine($"Endpoint B: {endpointDeviceInformationB.Name} ({endpointDeviceInformationB.EndpointDeviceId})");
            }

            // create the session

            var session = MidiSession.Create("UMP Endpoint Bridge");

            if (session == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Failed to create MIDI session."));
                return (int)MidiConsoleReturnCode.ErrorCreatingSession;
            }
            else
            {
                AnsiConsole.MarkupLine("MIDI session created successfully.");
            }

            // create both endpoints

            var connA = session.CreateEndpointConnection(endpointDeviceInformationA.EndpointDeviceId);
            var connB = session.CreateEndpointConnection(endpointDeviceInformationB.EndpointDeviceId);

            if (connA == null || connB == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("One or both endpoints could not be found. Error creating endpoint connections."));

                return (int)MidiConsoleReturnCode.ErrorCreatingEndpointConnection;
            }
            else
            {
                AnsiConsole.MarkupLine("Endpoint connections created successfully.");
            }

            // TODO: Add into SDK a circular connection detection function. Maybe it should send some MIDI 1 note-off
            // events and see if they come back. Add a parameter to not send this in case it causes issues. The check
            // should be per-group


            var bridge = new MidiEndpointConnectionBridge(connA, connB);


            // open the endpoints

            if (!connA.Open() || !connB.Open())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("One or both endpoints could not be opened."));

                return (int)MidiConsoleReturnCode.ErrorOpeningEndpointConnection;
            }
            else
            {
                AnsiConsole.MarkupLine("Endpoint connections opened successfully.");
            }


            // set console title

            Console.Title = $"MIDI Endpoint Bridge - {endpointDeviceInformationA.Name} <-> {endpointDeviceInformationB.Name}";


            // Wait for input

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


            bridge = null;

            session.Dispose();




            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
