using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Windows.Devices.Midi2.Endpoints.Loopback;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.Foundation;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class EndpointsLoopViewModel : SingleTransportEndpointViewModelBase, INavigationAware
    {
        private IMidiConfigFileService m_midiConfigFileService;

        public ICommand CreateLoopbackPairsCommand
        {
            get; private set;
        }


        public bool IsConfigFileActive
        {
            get { return m_midiConfigFileService.IsConfigFileActive; }
        }


        private string m_newLoopbackEndpointAName;
        public string NewLoopbackEndpointAName
        {
            get { return m_newLoopbackEndpointAName; }
            set
            {
                m_newLoopbackEndpointAName = value.Trim();
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        private string m_newLoopbackEndpointBName;
        public string NewLoopbackEndpointBName
        {
            get { return m_newLoopbackEndpointBName; }
            set
            {
                m_newLoopbackEndpointBName = value.Trim();
                UpdateValidState();
                OnPropertyChanged();
            }
        }

        public string NewUniqueIdentifier
        {
            get; set;
        }

        public bool NewLoopbackIsPersistent
        {
            get; set;
        }

        public bool NewLoopbackSettingsAreValid
        {
            get; internal set;
        }

        public string ValidationErrorMessage
        {
            get; internal set;
        }


        private void UpdateValidState()
        {
            // validate both names are not duplicates of each other

            // validate both names are not duplicates of any other endpoint name

            // validate the unique id is good

            // validate the unique id isn't already in use with another loopback

            // update NewLoopbackSettingsAreValid and ValidationErrorMessage if needed
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

        private void GenerateNewUniqueId()
        {
            var uniqueId = CleanupUniqueId(GuidHelper.CreateNewGuid().ToString());

            NewUniqueIdentifier = uniqueId;
        }

        private void CreateNewLoopbackEndpoints()
        {
            var endpointA = new MidiLoopbackEndpointDefinition();
            var endpointB = new MidiLoopbackEndpointDefinition();

            // if endpoint A or B names are empty, do not close. display an error

            // if endpoint A or B unique ids are empty, do not close. display suggestion to generate them
            // todo: need to limit to alpha plus just a couple other characters, and only 32 in length

            // descriptions are optional


            endpointA.Name = NewLoopbackEndpointAName.Trim();
            endpointB.Name = NewLoopbackEndpointBName.Trim();

            endpointA.UniqueId = CleanupUniqueId(NewUniqueIdentifier);
            endpointB.UniqueId = CleanupUniqueId(NewUniqueIdentifier);

            endpointA.Description = string.Empty;
            endpointB.Description = string.Empty;

            var associationId = GuidHelper.CreateNewGuid();

            var creationConfig = new MidiLoopbackEndpointCreationConfig(associationId, endpointA, endpointB);

            var result = MidiLoopbackEndpointManager.CreateTransientLoopbackEndpoints(creationConfig);

            // TODO: if that worked, and these are persistent, add to configuration file

            if (IsConfigFileActive && NewLoopbackIsPersistent)
            {
                if (result.Success)
                {
                    m_midiConfigFileService.StoreLoopbackEndpointPair(creationConfig);

                    RefreshDeviceCollection();
                }
                else
                {
                    // update error information
                }
            }
        }

        public EndpointsLoopViewModel(INavigationService navigationService, IMidiConfigFileService midiConfigFileService) : base("LOOP", navigationService)
        {
            m_midiConfigFileService = midiConfigFileService;

            GenerateNewUniqueId();

            CreateLoopbackPairsCommand = new RelayCommand(
                () =>
                {
                    CreateNewLoopbackEndpoints();

                });
        }

    }
}
