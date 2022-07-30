

using Microsoft.Windows.Midi;

namespace ClientSdkTests
{
    [TestClass]
    public class BasicTests
    {


        [TestMethod]
        public void TestPing()
        {
            string reportedVersion;

            MidiServicePingResponse response = MidiService.Ping(out reportedVersion);

            switch (response)
            {
                case MidiServicePingResponse.Error:
                    Assert.Fail("Ping: Error.");
                    break;
                case MidiServicePingResponse.TimeOut:
                    Assert.Fail("Ping: Time out.");
                    break;
                case MidiServicePingResponse.Pong:
                    // we're good
                    System.Diagnostics.Debug.WriteLine($"Ping: Server version returned: {reportedVersion}");
                    break;
            }
        }







    }
}