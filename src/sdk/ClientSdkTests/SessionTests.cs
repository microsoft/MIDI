using Microsoft.Windows.Midi.Session;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ClientSdkTests
{
    [TestClass]
    public class SessionTests
    {


        [TestMethod]
        public void CreateNewSession()
        {
            var session = MidiSession.Create("Test Session");

        }


    }
}
