using System.Text.Json;

namespace MidiSetup.Schema
{
    public class MidiSetup
    {

        public static MidiSetup Load(string fileName)
        {
            throw new NotImplementedException();
        }

        public static MidiSetup CreateEmpty()
        {
            throw new NotImplementedException();
        }




        // header
        public Header Header { get; set; }


        // transports

        // devices and endpoints



        public void Write()
        {
            // TODO: See if there is an easy way to write discrete changes to an existing doc
            // without parsing everything through jsondocument
        //    var options = new JsonSerializerOptions { AllowTrailingCommas = true, WriteIndented = true, } };
        }


    }
}