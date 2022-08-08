// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System.Diagnostics;
using System.Text.Json;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol
{
    public class MidiServiceConstants
    {

        private const string PipePrefix = "midi.";

        // May want to make this something which can be set in the config file

        public const string InitialConnectionPipeName = PipePrefix + "connect";
        public const string PingPipeName = PipePrefix + "ping";
        public const string EnumerationPipeName = PipePrefix + "enumeration";


        public const string EnumerationServicePipePrefix = PipePrefix + "enumerator";

        public const string SessionServicePipePrefix = PipePrefix + "session";  // this gets an ID added to it



    }
}