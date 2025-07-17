// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Data.Json;

using Microsoft.Windows.Devices.Midi2.ServiceConfig;

namespace Microsoft.Midi.Settings.Config
{
    internal partial class EndpointConfigMetadataPayload : IMidiServiceTransportPluginConfig
    {
        public bool IsFromCurrentConfigFile => true;


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


        public string GetConfigJson()
        {

            // TODO







            return string.Empty;
        }

    }
}
