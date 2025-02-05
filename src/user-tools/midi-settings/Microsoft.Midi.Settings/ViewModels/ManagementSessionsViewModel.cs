using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using CommunityToolkit.Mvvm.ComponentModel;

using Microsoft.Windows.Devices.Midi2.Diagnostics;
using WinUIEx;


namespace Microsoft.Midi.Settings.ViewModels
{
    public class ManagementSessionsViewModel : ObservableRecipient
    {

        public ObservableCollection<MidiServiceSessionInformationWrapper> Sessions { get; private set; } = [];

        public ManagementSessionsViewModel()
        {
            RefreshSessions();
        }


        public void RefreshSessions()
        {
            Sessions.Clear();

            var sessions = MidiReporting.GetActiveSessions();

            foreach (var session in sessions) 
            {
                var wrapper = new MidiServiceSessionInformationWrapper();

                wrapper.SessionInfo = session;

                foreach (var conn in session.Connections) 
                {

                    var connectionWrapper = new MidiServiceSessionConnectionInfoWrapper();

                    connectionWrapper.ConnectionInfo = conn;

                    // look up name

                    var di = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(conn.EndpointDeviceId);
                    if (di != null)
                    {
                        connectionWrapper.EndpointName = di.Name;
                    }
                    else
                    {
                        connectionWrapper.EndpointName = "Unknown";
                    }

                    wrapper.SessionConnections.Add(connectionWrapper);
                }

                Sessions.Add(wrapper);
            }

        }
    }
}
