using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.Midi.Settings.Services;
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System.Xml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings
{
    /// <summary>
    /// An empty window that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class CreateLoopbackEndpointsWindow : WinUIEx.WindowEx
    {
        public event EventHandler EndpointsCreated;

        public CreateLoopbackEndpointsWindow()
        {
            this.Title = "CreateLoopbackEndpointsWindowTitle".GetLocalized();

            // TODO: This shouldn't be hard-coded
            this.CenterOnScreen(650, 750);

            this.InitializeComponent();

        //    this.ContentRoot.RequestedTheme = App.GetService<ThemeSelectorService>().Theme;
        }

        private void CreateEndpointsButton_Click(object sender, RoutedEventArgs e)
        {
            var endpointA = new MidiLoopbackEndpointDefinition();
            var endpointB = new MidiLoopbackEndpointDefinition();

            // if endpoint A or B names are empty, do not close the window. display an error

            // if endpoint A or B unique ids are empty, do not close the window. display suggestion to generate them
            // todo: need to limit to alpha plus just a couple other characters, and only 32 in length

            // descriptions are optional


            endpointA.Name = EndpointAName.Text.Trim();
            endpointB.Name = EndpointBName.Text.Trim();

            endpointA.UniqueId = CleanupUniqueId(EndpointAUniqueId.Text);
            endpointB.UniqueId = CleanupUniqueId(EndpointBUniqueId.Text);

            EndpointAUniqueId.Text = endpointA.UniqueId;
            EndpointBUniqueId.Text = endpointB.UniqueId;

            endpointA.Description = EndpointADescription.Text.Trim();
            endpointB.Description = EndpointBDescription.Text.Trim();

            var associationId = GuidHelper.CreateNewGuid();

            var creationConfig = new MidiLoopbackEndpointCreationConfig(associationId, endpointA, endpointB);

            var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

            // TODO: if that worked, and these are persistent, add to configuration file

            if (result.Success)
            {
                // close the window
                Close();

                if (EndpointsCreated != null)
                {
                    EndpointsCreated(this, new EventArgs());
                }
            }
            else
            {
                // display failure message. Do not close window
                
            }


        }

        private string CleanupUniqueId(string source)
        {
            string result = string.Empty;

            foreach (char ch in source.ToCharArray())
            {
                if (char.IsAsciiLetterOrDigit(ch))
                {
                    result += ch;
                }
            }

            return result;
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void GenerateEndpointAUniqueIdButton_Click(object sender, RoutedEventArgs e)
        {
            var uniqueId = CleanupUniqueId(GuidHelper.CreateNewGuid().ToString());

            EndpointAUniqueId.Text = uniqueId;

            if (string.IsNullOrEmpty(EndpointBUniqueId.Text.Trim()))
            {
                EndpointBUniqueId.Text = uniqueId;
            }
        }

        private void GenerateEndpointBUniqueIdButton_Click(object sender, RoutedEventArgs e)
        {
            var uniqueId = CleanupUniqueId(GuidHelper.CreateNewGuid().ToString());

            EndpointBUniqueId.Text = uniqueId;

            if (string.IsNullOrEmpty(EndpointAUniqueId.Text.Trim()))
            {
                EndpointAUniqueId.Text = uniqueId;
            }
        }
    }
}
