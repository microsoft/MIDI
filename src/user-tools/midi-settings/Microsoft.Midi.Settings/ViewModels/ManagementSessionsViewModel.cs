// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Windows.Devices.Midi2.Diagnostics;
using WinUIEx;


namespace Microsoft.Midi.Settings.ViewModels
{
    public class ManagementSessionsViewModel : ObservableRecipient, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "sessions", "open connections" };
        }

        public static string GetSearchPageTitle()
        {
            return "Session Management";
        }

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
