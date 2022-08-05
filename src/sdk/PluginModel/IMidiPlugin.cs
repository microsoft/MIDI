// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft.Windows.Midi.PluginModel
{

    public enum MidiPluginType
    {
        MessageProcessor = 100,
        Transport = 200,
    }

    public interface IMidiPlugin
    {
        MidiPluginType PluginType { get; }

        string PluginAuthor { get; }

        string PluginIconFileName { get; }

        string PluginDescription { get; }

        //string TypeName { get; }

        Version PluginVersion { get; }

    }
}