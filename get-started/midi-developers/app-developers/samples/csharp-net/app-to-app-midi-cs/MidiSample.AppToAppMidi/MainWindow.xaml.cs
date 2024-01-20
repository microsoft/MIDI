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

using midi2 = Windows.Devices.Midi2;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace MidiSample.AppToAppMidi
{
    /// <summary>
    /// An empty window that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainWindow : Window
    {
        private midi2.MidiSession _session;

        private midi2.MidiEndpointConnection _connection;


        public MainWindow()
        {
            this.InitializeComponent();

            this.Closed += MainWindow_Closed;

            this.AppWindow.MoveAndResize(new Windows.Graphics.RectInt32(100, 100, 600, 600));

            OpenConnection();

        }

        private void MainWindow_Closed(object sender, WindowEventArgs args)
        {
            _session.Dispose();
        }

        private void OpenConnection()
        {
            _session = midi2.MidiSession.CreateSession("App to app MIDI sample");

            _connection = _session.CreateEndpointConnection(midi2.MidiEndpointDeviceInformation.DiagnosticsLoopbackAEndpointId);

            _connection.Open();
        }



        private byte GetMidiNoteNumberFromPad(Rectangle pad)
        {
            // you could do anything here. We're just going to use the number that's in the tag

            return byte.Parse(pad.Tag!.ToString());
        }

        private void OnPadPointerPressed(object sender, PointerRoutedEventArgs e)
        {
            byte note = GetMidiNoteNumberFromPad((Rectangle)sender);

            var message = midi2.MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(0, 0, midi2.Midi2ChannelVoiceMessageStatus.NoteOn, 0, note, 1000);

            _connection.SendMessagePacket(message);

            //System.Diagnostics.Debug.WriteLine($"Note on {note}");
        }

        private void OnPadPointerReleased(object sender, PointerRoutedEventArgs e)
        {
            byte note = GetMidiNoteNumberFromPad((Rectangle)sender);

            var message = midi2.MidiMessageBuilder.BuildMidi2ChannelVoiceMessage(0, 0, midi2.Midi2ChannelVoiceMessageStatus.NoteOff, 0, note, 1000);

            _connection.SendMessagePacket(message);

            //System.Diagnostics.Debug.WriteLine($"Note off {note}");
        }
    }
}
