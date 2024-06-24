using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Data.Json;


namespace Microsoft.Midi.Settings.Config
{
    internal class EndpointConfigMetadataPayload : IMidiServiceTransportPluginConfiguration
    {
        public bool IsFromConfigurationFile => true;

        public JsonObject? SettingsJson { get; private set; }

        public Guid TransportId { get; private set; }

        private readonly string _newName;
        private readonly string _newDescription;
        private readonly string _newSmallImagePath;
        private readonly string _newLargeImagePath;

        public EndpointConfigMetadataPayload(Guid transportId, string newName, string newDescription, string newSmallImagePath, string newLargeImagePath)
        {
            TransportId = transportId;

            _newName = newName.Trim();
            _newDescription = newDescription.Trim();
            _newSmallImagePath = newSmallImagePath.Trim();
            _newLargeImagePath = newLargeImagePath.Trim();
        }

        // build the  json for endpoint properties updates
        // "endpointTransportPluginSettings":
        // {
        //   abstraction guid :
        //   {
        //     "update"
        //     [
        //        {
        //           "match":
        //           [
        //              {
        //                  "match_prop_name" : "match_value"
        //              }
        //           ]
        //           "prop_name" : "value"
        //        }
        //     ]
        //   }
        // }

        public void BuildJson()
        {

        }
    }
}
