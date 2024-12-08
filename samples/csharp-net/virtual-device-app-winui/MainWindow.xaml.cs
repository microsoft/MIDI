using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.UI.Xaml.Shapes;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.AI.MachineLearning;
using Windows.Foundation;
using Windows.Foundation.Collections;
using WinUIEx;

using Microsoft.Windows.Devices.Midi2;
using Microsoft.Windows.Devices.Midi2.Messages;
using Microsoft.Windows.Devices.Midi2.Endpoints.Virtual;
//using Microsoft.Windows.Devices.Midi2.Initialization;
using Windows.UI.Popups;


namespace MidiSample.AppToAppMidi
{

    public sealed partial class MainWindow : Microsoft.UI.Xaml.Window
    {      
        private MidiSession _session;
        private MidiEndpointConnection _connection;
        private MidiVirtualDevice _virtualDevice;

        private Microsoft.Windows.Devices.Midi2.Initialization.MidiDesktopAppSdkInitializer _initializer;

        public List<Note> Notes { get; }

        public MainWindow()
        {
            this.InitializeComponent();

            UpdateName.IsEnabled = false;
            EndpointNameEntry.IsEnabled = false;
            PadContainer.Visibility = Visibility.Collapsed;

            this.SetWindowSize(500, 600);
            this.Closed += MainWindow_Closed;

            // the initializer implements the Dispose pattern, so will shut down WinRT type redirection when disposed
            _initializer = Microsoft.Windows.Devices.Midi2.Initialization.MidiDesktopAppSdkInitializer.Create();

            // initialize SDK runtime
            if (_initializer.InitializeSdkRuntime())
            {
                // start the service
                if (_initializer.EnsureServiceAvailable())
                {
                    if (StartVirtualDevice())
                    {
                        var notes = new byte[] { 50, 52, 53, 55, 57, 58, 60, 62, 64, 65, 67, 69, 70, 72, 74, 76 };
                        Notes = notes.Select(n => new Note() { NoteNumber = n, Connection = _connection, GroupIndex = 0, ChannelIndex = 0 }).ToList();

                        this.SetIsAlwaysOnTop(true);

                        UpdateName.IsEnabled = true;
                        EndpointNameEntry.IsEnabled = true;
                        PadContainer.Visibility = Visibility.Visible;
                    }
                    else
                    {
                        this.AppWindow.Title = "(failed to start virtual device)";
                    }
                }
                else
                {
                    this.AppWindow.Title = "(failed to get service running)";
                    Console.WriteLine("!Failed to get service running");
                }
            }
            else
            {
                this.AppWindow.Title = "(failed to get initialize SDK runtime)";
                Console.WriteLine("!Failed to initialize SDK Runtime");
            }

        }

        private void MainWindow_Closed(object sender, WindowEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("MainWindow_Closed");

            if (_connection != null)
            {
                _session.DisconnectEndpointConnection(_connection.ConnectionId);
            }
            _session.Dispose();

            if (_initializer != null)
            {
                _initializer.Dispose();
            }
        }

        private bool StartVirtualDevice()
        {
            try
            {
                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Connection enter");

                // define our virtual device
                var creationConfig = DefineDevice();

                // create the session. The name here is just convenience.
                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Creating session");
                _session = MidiSession.Create(creationConfig.Name);

                // return if unable to create session
                if (_session == null)
                {
                    System.Diagnostics.Debug.WriteLine("StartVirtualDevice Unable to create session");
                    return false;
                }

                // create the virtual device, so we can get the endpoint device id to connect to
                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Creating virtual device");
                _virtualDevice = MidiVirtualDeviceManager.CreateVirtualDevice(creationConfig);


                // return if unable to create virtual device
                if (_virtualDevice == null)
                {
                    System.Diagnostics.Debug.WriteLine("StartVirtualDevice Unable to create virtual device");
                    return false;
                }

                // this is for debugging in the sample. Normally you'd have this set to true
                // you want to set this before you open the "device" side connection or else you may
                // miss the initial discovery messages
                _virtualDevice.SuppressHandledMessages = false;


                // create our device-side connection
                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Creating endpoint connection");
                _connection = _session.CreateEndpointConnection(_virtualDevice.DeviceEndpointDeviceId);

                if (_connection == null)
                {
                    System.Diagnostics.Debug.WriteLine("StartVirtualDevice failed to create connection");
                    return false;
                }

                // necessary for the virtual device to participate in MIDI communication
                _connection.AddMessageProcessingPlugin(_virtualDevice);

                // wire up the stream configuration request received handler
                _virtualDevice.StreamConfigRequestReceived += OnStreamConfigurationRequestReceived;

                // wire up the message received handler on the connection itself
                _connection.MessageReceived += OnMidiMessageReceived;

                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Opening connection");
                if (_connection.Open())
                {
                    System.Diagnostics.Debug.WriteLine("Connection Opened");

                    this.AppWindow.Title = creationConfig.Name + ": Connected";

                    return true;
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Connection Open Failed");

                    return false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("Exception: " + ex.ToString());

                return false;
            }
        }

        private void OnMidiMessageReceived(IMidiMessageReceivedEventSource sender, MidiMessageReceivedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine($"- Received: {MidiMessageHelper.GetMessageDisplayNameFromFirstWord(args.PeekFirstWord())} / {args.GetMessagePacket().ToString()}");
        }

        private void OnStreamConfigurationRequestReceived(MidiVirtualDevice sender, MidiStreamConfigRequestReceivedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("Stream configuration request received");
        }


        MidiVirtualDeviceCreationConfig DefineDevice()
        {
            // some of these values may seem redundant, but for physical devices
            // they are all sourced from different locations, and we want virtual
            // devices to behave like physical devices.

            string userSuppliedName = "Pad Controller App";
            string userSuppliedDescription = "My favorite demo app for Windows MIDI Services";

            string transportSuppliedName = "Pad Controller";
            string transportSuppliedDescription = "A sample app-to-app MIDI virtual device";
            string transportSuppliedManufacturerName = "Constoso";

            string endpointSuppliedName = transportSuppliedName;




            EndpointNameEntry.Text = endpointSuppliedName;


            var declaredEndpointInfo = new MidiDeclaredEndpointInfo();
            declaredEndpointInfo.Name = endpointSuppliedName;
            declaredEndpointInfo.ProductInstanceId = "PMB_APP2_3263827";
            declaredEndpointInfo.SpecificationVersionMajor = 1; // see latest MIDI 2 UMP spec
            declaredEndpointInfo.SpecificationVersionMinor = 1; // see latest MIDI 2 UMP spec
            declaredEndpointInfo.SupportsMidi10Protocol = false;
            declaredEndpointInfo.SupportsMidi20Protocol = true;
            declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps = false;
            declaredEndpointInfo.SupportsSendingJitterReductionTimestamps = false;

            declaredEndpointInfo.HasStaticFunctionBlocks = true;
            declaredEndpointInfo.DeclaredFunctionBlockCount = 3;    // needs to match the number of function blocks in the collection


            // todo: set any device identity values if you want. This is optional
            // The SysEx id, if used, needs to be a valid one
            var declaredDeviceIdentity = new MidiDeclaredDeviceIdentity() ;
            declaredDeviceIdentity.DeviceFamilyMsb = 0x01;
            declaredDeviceIdentity.DeviceFamilyLsb = 0x02;
            declaredDeviceIdentity.DeviceFamilyModelNumberMsb = 0x03;
            declaredDeviceIdentity.DeviceFamilyModelNumberLsb = 0x04;
            declaredDeviceIdentity.SoftwareRevisionLevelByte1 = 0x05;
            declaredDeviceIdentity.SoftwareRevisionLevelByte2 = 0x06;
            declaredDeviceIdentity.SoftwareRevisionLevelByte3 = 0x07;
            declaredDeviceIdentity.SoftwareRevisionLevelByte4 = 0x08;
            declaredDeviceIdentity.SystemExclusiveIdByte1 = 0x09;
            declaredDeviceIdentity.SystemExclusiveIdByte2 = 0x0A;
            declaredDeviceIdentity.SystemExclusiveIdByte3 = 0x0B;


            var userSuppliedInfo = new MidiEndpointUserSuppliedInfo() ;
            userSuppliedInfo.Name = userSuppliedName;           // for names, this will bubble to the top in priority
            userSuppliedInfo.Description = userSuppliedDescription;


            var config = new MidiVirtualDeviceCreationConfig(
                transportSuppliedName,                          // this could be a different "transport-supplied" name value here
                transportSuppliedDescription,                   // transport-supplied description
                transportSuppliedManufacturerName,              // transport-supplied company name
                declaredEndpointInfo,                           // for endpoint discovery
                declaredDeviceIdentity,                         // for endpoint discovery
                userSuppliedInfo    
            );

            // if set to true, WinMM / WinRT MIDI 1.0 backwards-compat endpoints won't be created
            // when that feature is available in the service
            config.CreateOnlyUmpEndpoints = false;

            // Function blocks. The MIDI 2 UMP specification covers the meanings
            // of these values

            // Note: the number of blocks needs to match the number declared for the endpoint
            // and function blocks must start at 0 and go up from there, without any gaps

            var block1 = new MidiFunctionBlock();
            block1.Number = 0;
            block1.Name = "Pads Output";
            block1.IsActive = true;
            block1.UIHint = MidiFunctionBlockUIHint.Sender;
            block1.FirstGroup = new MidiGroup(0);
            block1.GroupCount = 1;
            block1.Direction = MidiFunctionBlockDirection.Bidirectional;
            block1.RepresentsMidi10Connection = MidiFunctionBlockRepresentsMidi10Connection.Not10;
            block1.MaxSystemExclusive8Streams = 0;
            block1.MidiCIMessageVersionFormat = 0;

            config.FunctionBlocks.Add(block1);

            var block2 = new MidiFunctionBlock();
            block2.Number = 1;
            block2.Name = "Second Function Block";
            block2.IsActive = true;
            block2.UIHint = MidiFunctionBlockUIHint.Sender;
            block2.FirstGroup = new MidiGroup(1);
            block2.GroupCount = 4;
            block2.Direction = MidiFunctionBlockDirection.Bidirectional;
            block2.RepresentsMidi10Connection = MidiFunctionBlockRepresentsMidi10Connection.Not10;
            block2.MaxSystemExclusive8Streams = 0;
            block2.MidiCIMessageVersionFormat = 0;

            config.FunctionBlocks.Add(block2);

            var block3 = new MidiFunctionBlock();
            block3.Number = 2;
            block3.Name = "Third Function Block";
            block3.IsActive = false;                // function blocks can be marked as inactive.
            block3.UIHint = MidiFunctionBlockUIHint.Receiver;
            block3.FirstGroup = new MidiGroup(5);
            block3.GroupCount = 1;
            block3.Direction = MidiFunctionBlockDirection.BlockInput;
            block3.RepresentsMidi10Connection = MidiFunctionBlockRepresentsMidi10Connection.Not10;
            block3.MaxSystemExclusive8Streams = 0;
            block3.MidiCIMessageVersionFormat = 0;

            config.FunctionBlocks.Add(block3);

            return config;
        }

        private void UpdateName_Click(object sender, RoutedEventArgs e)
        {
            var newName = EndpointNameEntry.Text.Trim();

            if (!string.IsNullOrEmpty(newName))
            {
                if (_virtualDevice.UpdateEndpointName(newName))
                {
                    EndpointNameEntry.Text = newName;
                }
                else
                {
                    // failed to update name
                }

            }
        }
    }
}
