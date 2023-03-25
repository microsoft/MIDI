// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Service.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Model
{
    // This graph gets written out to shared memory so the management service
    // can inspect it and see all open sessions
    public class SessionGraph
    {
        // guid-indexed collection of sessions
        private Dictionary<Guid, MidiSessionState> _sessions =
            new Dictionary<Guid, MidiSessionState>();

        private readonly ILogger _logger;
         

        public SessionGraph(ILogger<ConnectionService> logger)
        {
            _logger = logger;
        }



        private void PublishGraph()
        {

        }
        

        internal Guid GenerateNewSessionId()
        {
            return Guid.NewGuid();
        }

        // pipe names max out at 256 characters
        internal String GenerateSessionChannelName(Guid sessionId, long processId)
        {
            return MidiServiceConstants.SessionServicePipePrefix + "_" + processId.ToString("D10") + "_" + sessionId.ToString("N");
        }

        internal void AddSession(MidiSessionState sessionToAdd)
        {
            lock (_sessions)
            {
                _sessions.Add(sessionToAdd.Id, sessionToAdd);
            }
        }

        internal void RemoveSession(Guid sessionId)
        {
            lock (_sessions)
            {

            }

        }
    }
}
