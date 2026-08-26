// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Text.Json.Nodes;
using Windows.Devices.Midi2.ServiceConfig;

namespace Microsoft.Midi.ConsoleApp
{
    // A loopback which is running but not in the configuration file goes away with the service,
    // which is the single most useful thing to know when looking at the list.
    internal static class LoopbackConfigReader
    {
        private const string TransportPluginSettingsKey = "endpointTransportPluginSettings";
        private const string CreateKey = "create";

        internal static HashSet<Guid> GetSavedAssociationIds(Guid transportId)
        {
            var results = new HashSet<Guid>();

            try
            {
                var path = MidiServiceTransportPluginConfigManager.ConfigFilePath;

                if (string.IsNullOrEmpty(path) || !File.Exists(path))
                {
                    return results;
                }

                if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject root ||
                    root[TransportPluginSettingsKey] is not JsonObject pluginSettings)
                {
                    return results;
                }

                foreach (var transport in pluginSettings)
                {
                    if (!Guid.TryParse(transport.Key, out var key) || key != transportId)
                    {
                        continue;
                    }

                    if (transport.Value is not JsonObject section || section[CreateKey] is not JsonObject created)
                    {
                        continue;
                    }

                    foreach (var entry in created)
                    {
                        if (Guid.TryParse(entry.Key, out var associationId))
                        {
                            results.Add(associationId);
                        }
                    }
                }
            }
            catch (Exception)
            {
                // an unreadable configuration file just means nothing can be reported as saved
            }

            return results;
        }
    }
}
