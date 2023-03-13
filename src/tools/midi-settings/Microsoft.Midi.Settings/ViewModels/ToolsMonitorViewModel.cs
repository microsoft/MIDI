using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Models;
using Microsoft.Midi.Settings.Core.Services;
using Microsoft.Midi.Settings.Services;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class ToolsMonitorViewModel : ObservableRecipient, INavigationAware
    {
        private readonly INavigationService _navigationService;
        private readonly ISampleDataService _sampleDataService;

        public ObservableCollection<DisplayFriendlyMidiMessage> Messages { get; } = new ObservableCollection<DisplayFriendlyMidiMessage>();
        public ObservableCollection<string> UmpEndpointNames { get; } = new ObservableCollection<string>();
        public ObservableCollection<string> AllGroups { get; } = new ObservableCollection<string>();
        public ObservableCollection<string> AllChannels { get; } = new ObservableCollection<string>();

        public ToolsMonitorViewModel(INavigationService navigationService, ISampleDataService sampleDataService)
        {
            _navigationService = navigationService;
            _sampleDataService = sampleDataService;

            //        ItemClickCommand = new RelayCommand<SampleOrder>(OnItemClick);
        }



        public async void OnNavigatedTo(object parameter)
        {
            Messages.Clear();
            UmpEndpointNames.Clear();

            // TODO: Replace with real data.

            var messages = await _sampleDataService.GetUmpMonitorDataAsync();

            foreach (var item in messages)
            {
                Messages.Add(item);
            }

            var endpoints = await _sampleDataService.GetUmpEndpointNamesAsync();

            foreach (var item in endpoints)
            {
                UmpEndpointNames.Add(item);
            }

            AllGroups.Add("All (Keys, Tone Generator, Sequencer)");
            AllChannels.Add("All (1-16)");

            for (int i = 1; i <= 16; i++) 
            { 
                AllGroups.Add(i.ToString());
                AllChannels.Add(i.ToString());
            }

        }

        public void OnNavigatedFrom()
        {
        }
    }
}
