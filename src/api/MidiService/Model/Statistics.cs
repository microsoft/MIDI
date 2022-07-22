using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Model
{
    public class Statistics
    {
        // TODO: Should we capture any time information with these?
        // TODO: The methods here should check the session's current log level

        /// <summary>
        /// the string key is the typename of the message
        /// </summary>
        private Dictionary<string, long> _sessionServiceIncomingMessagesStatistics
                = new Dictionary<string, long>();

        /// <summary>
        /// the string key is the typename of the message
        /// </summary>
        private Dictionary<string, long> _SessionServiceOutgoingMessagesStatistics
                = new Dictionary<string, long>();




        //// TODO: Implement this so it takes the type, generates the key. Be sure to check loglevel
        //public void LogSessionServiceIncomingMessage(MidiSession session, ProtocolMessage message)
        //{

        //}




    }
}
