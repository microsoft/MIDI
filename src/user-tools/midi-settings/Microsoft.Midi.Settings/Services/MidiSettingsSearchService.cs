using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    class MidiSettingsSearchService : IMidiSettingsSearchService
    {

        private List<MidiSettingsSearchResult> AllItems = [];

        private void Refresh()
        {
            // build the initial search list
            AllItems.Clear();

            // add all pages. Target is typeof(viewmodel).FullName



            // add all endpoints and their various names

            // add all transports

            // add all current session names


        }

        public MidiSettingsSearchService()
        {
            Refresh();
        }

        public IList<MidiSettingsSearchResult> GetFilteredResults(string filterText)
        {







            return AllItems;
        }
    }
}
