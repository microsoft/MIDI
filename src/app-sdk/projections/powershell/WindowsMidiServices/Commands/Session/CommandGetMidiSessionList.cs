using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiSessionList")]
    public class CommandGetMidiSessionList : Cmdlet
    {
        protected override void ProcessRecord()
        {
            var sdkSessions = Microsoft.Windows.Devices.Midi2.Reporting.MidiReporting.GetActiveSessions();

            List<MidiSessionInfo> sessions = [];

            foreach (var sdkSession in sdkSessions)
            {
                sessions.Add(new MidiSessionInfo(sdkSession));
            }

            WriteObject(sessions.AsReadOnly());
        }
    }


}