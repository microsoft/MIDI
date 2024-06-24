using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

using midi2 = Microsoft.Windows.Devices.Midi2;
using msgs = Microsoft.Windows.Devices.Midi2.Messages;
using virt = Microsoft.Windows.Devices.Midi2.Endpoints.Virtual;

namespace virtual_device_app_wpf
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private midi2.MidiSession _session;
        private midi2.MidiEndpointConnection _connection;

        public List<Note> Notes { get; }

        public MainWindow()
        {
            this.InitializeComponent();

            OpenConnection();

            var notes = new byte[] { 50, 52, 53, 55, 57, 58, 60, 62, 64, 65, 67, 69, 70, 72, 74, 76 };

            Notes = notes.Select(n => new Note() { NoteNumber = n, Connection = _connection, GroupIndex = 0, ChannelIndex = 0 }).ToList();

            this.Closed += MainWindow_Closed;

            this.Height = 550;
            this.Width = 500;
            this.Topmost = true;
        }

        private void MainWindow_Closed(object? sender, EventArgs e)
        {
            System.Diagnostics.Debug.WriteLine("MainWindow_Closed");

            if (_connection != null)
            {
                _session.DisconnectEndpointConnection(_connection.ConnectionId);
            }

            _session.Dispose();
        }

        private void OpenConnection()
        {
            try
            {
                System.Diagnostics.Debug.WriteLine("Open Connection enter");


                // create our function blocks and endpoint info to be reported back through MIDI

                var deviceDefinition = new midi2.MidiVirtualEndpointDeviceDefinition();

                deviceDefinition.FunctionBlocks.Add(new midi2.MidiFunctionBlock()
                {
                    Number = 0,
                    IsActive = true,
                    Name = "Pads Output",
                    UIHint = midi2.MidiFunctionBlockUIHint.Sender,
                    FirstGroupIndex = 0,
                    GroupCount = 1,
                    Direction = midi2.MidiFunctionBlockDirection.Bidirectional,
                    Midi10Connection = midi2.MidiFunctionBlockMidi10.Not10,
                    MaxSystemExclusive8Streams = 0,
                    MidiCIMessageVersionFormat = 0
                });

                deviceDefinition.FunctionBlocks.Add(new midi2.MidiFunctionBlock()
                {
                    Number = 1,
                    IsActive = true,
                    Name = "A Function Block",
                    UIHint = midi2.MidiFunctionBlockUIHint.Sender,
                    FirstGroupIndex = 1,
                    GroupCount = 1,
                    Direction = midi2.MidiFunctionBlockDirection.Bidirectional,
                    Midi10Connection = midi2.MidiFunctionBlockMidi10.Not10,
                    MaxSystemExclusive8Streams = 0,
                    MidiCIMessageVersionFormat = 0
                });

                deviceDefinition.AreFunctionBlocksStatic = true;
                deviceDefinition.EndpointName = "Pad Controller App";
                deviceDefinition.EndpointProductInstanceId = "PMB_APP2_3263827";
                deviceDefinition.SupportsMidi2ProtocolMessages = true;
                deviceDefinition.SupportsMidi1ProtocolMessages = true;
                deviceDefinition.SupportsReceivingJRTimestamps = false;
                deviceDefinition.SupportsSendingJRTimestamps = false;


                System.Diagnostics.Debug.WriteLine("Creating session");
                _session = midi2.MidiSession.CreateSession(deviceDefinition.EndpointName);

                if (_session != null)
                {
                    System.Diagnostics.Debug.WriteLine("Creating virtual device");
                    _connection = _session.CreateVirtualDeviceAndConnection(deviceDefinition);

                    if (_connection != null)
                    {
                        System.Diagnostics.Debug.WriteLine("Created endpoint id: " + _connection.EndpointDeviceId);
                        System.Diagnostics.Debug.WriteLine("Connection created. Wiring up MessageReceived event");

                        _connection.MessageReceived += _connection_MessageReceived;

                        // do anything else needed here. The public endpoint is not available to other
                        // applications until you open the device endpoint

                        System.Diagnostics.Debug.WriteLine("Connection created. About to open it.");

                        if (_connection.Open())
                        {
                            System.Diagnostics.Debug.WriteLine("Connection Opened");

                            this.Title = deviceDefinition.EndpointName + ": Connected";
                        }
                        else
                        {
                            System.Diagnostics.Debug.WriteLine("Connection Open Failed");
                            this.Title = deviceDefinition.EndpointName + ": (no connection)";
                        }
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Returned connection is null");
                    }
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Session Open Failed");
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("Exception: " + ex.ToString());
            }
        }

        private void _connection_MessageReceived(midi2.IMidiMessageReceivedEventSource sender, midi2.MidiMessageReceivedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine("Message Received " + msgs.MidiMessageUtility.GetMessageDisplayNameFromFirstWord(
                args.PeekFirstWord()));
        }

    }
}