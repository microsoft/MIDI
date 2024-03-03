using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;

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

            var sessions = MidiService.GetActiveSessions();

            foreach (var session in sessions) 
            {
                var wrapper = new MidiServiceSessionInformationWrapper();

                wrapper.SessionInfo = session;

                foreach (var conn in session.Connections) 
                { 
                    wrapper.SessionConnections.Add(conn);
                }

                Sessions.Add(wrapper);
            }

        }
    }
}
