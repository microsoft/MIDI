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
using Microsoft.Windows.Devices.Midi2.Initialization;


namespace MidiSample.AppToAppMidi
{

    public sealed partial class MainWindow : Microsoft.UI.Xaml.Window
    {      
        private MidiSession _session;
        private MidiEndpointConnection _connection;
        private MidiVirtualDevice _virtualDevice;

        public List<Note> Notes { get; }

        public MainWindow()
        {
            this.InitializeComponent();

            if (!MidiServicesInitializer.EnsureServiceAvailable())
            {
                // In your application, you may decide it is appropriate to fall back to an older MIDI API
                Console.WriteLine("Windows MIDI Services is not available");
            }
            else
            {
                // bootstrap the SDK runtime. Should check the return result here
                MidiServicesInitializer.InitializeSdkRuntime();

                StartVirtualDevice();

                var notes = new byte[] { 50, 52, 53, 55, 57, 58, 60, 62, 64, 65, 67, 69, 70, 72, 74, 76 };

                Notes = notes.Select(n => new Note() { NoteNumber = n, Connection = _connection, GroupIndex = 0, ChannelIndex = 0 }).ToList();
            }

            this.Closed += MainWindow_Closed;

            this.SetWindowSize(500, 600);
            this.SetIsAlwaysOnTop(true);
        }

        private void MainWindow_Closed(object sender, WindowEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("MainWindow_Closed");

            if (_connection != null)
            {
                _session.DisconnectEndpointConnection(_connection.ConnectionId);
            }

            _session.Dispose();
        }

        private void StartVirtualDevice()
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
                    return;
                }

                // create the virtual device, so we can get the endpoint device id to connect to
                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Creating virtual device");
                _virtualDevice = MidiVirtualDeviceManager.CreateVirtualDevice(creationConfig);

                // return if unable to create virtual device
                if (_virtualDevice == null)
                {
                    System.Diagnostics.Debug.WriteLine("StartVirtualDevice Unable to create virtual device");
                    return;
                }

                // create our device-side connection
                System.Diagnostics.Debug.WriteLine("StartVirtualDevice Creating endpoint connection");
                _connection = _session.CreateEndpointConnection(_virtualDevice.DeviceEndpointDeviceId);

                if (_connection == null)
                {
                    System.Diagnostics.Debug.WriteLine("StartVirtualDevice failed to create connection");
                    return;
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
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Connection Open Failed");
                    this.AppWindow.Title = creationConfig.Name + ": (no connection)";
                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("Exception: " + ex.ToString());
            }
        }

        private void OnMidiMessageReceived(IMidiMessageReceivedEventSource sender, MidiMessageReceivedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("Message Received " + MidiMessageHelper.GetMessageDisplayNameFromFirstWord(
                args.PeekFirstWord()));
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
            declaredEndpointInfo.SupportsMidi10Protocol = true;
            declaredEndpointInfo.SupportsMidi20Protocol = true;
            declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps = false;
            declaredEndpointInfo.SupportsSendingJitterReductionTimestamps = false;
            declaredEndpointInfo.HasStaticFunctionBlocks = true;
            

            var declaredDeviceIdentity = new MidiDeclaredDeviceIdentity() ;
            // todo: set any device identity values if you want. This is optional

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
            config.CreateOnlyUmpEndpoints = false;

            // Function blocks. The MIDI 2 UMP specification covers the meanings
            // of these values
            var block1 = new MidiFunctionBlock();
            block1.Number = 0;
            block1.Name = "Pads Output";
            block1.IsActive = true;
            block1.UIHint = MidiFunctionBlockUIHint.Sender;
            block1.FirstGroupIndex = 0;
            block1.GroupCount = 1;
            block1.Direction = MidiFunctionBlockDirection.Bidirectional;
            block1.RepresentsMidi10Connection = MidiFunctionBlockRepresentsMidi10Connection.Not10;
            block1.MaxSystemExclusive8Streams = 0;
            block1.MidiCIMessageVersionFormat = 0;

            config.FunctionBlocks.Add(block1);

            var block2 = new MidiFunctionBlock();
            block2.Number = 1;
            block2.Name = "A Function Block";
            block2.IsActive = true;
            block2.UIHint = MidiFunctionBlockUIHint.Sender;
            block2.FirstGroupIndex = 1;
            block2.GroupCount = 2;
            block2.Direction = MidiFunctionBlockDirection.Bidirectional;
            block2.RepresentsMidi10Connection = MidiFunctionBlockRepresentsMidi10Connection.Not10;
            block2.MaxSystemExclusive8Streams = 0;
            block2.MidiCIMessageVersionFormat = 0;

            config.FunctionBlocks.Add(block2);

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
