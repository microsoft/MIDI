using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    public class MessageReceivedEventArgs : EventArgs
    {
        public UInt64 Timestamp;
        public UInt32[] Words;

        public MessageReceivedEventArgs(UInt64 timestamp, UInt32[] words)
        {
            Timestamp = timestamp;
            Words = words;
        }
    }
}
