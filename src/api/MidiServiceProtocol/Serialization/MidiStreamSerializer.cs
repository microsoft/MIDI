using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml.Serialization;
using System.Threading.Tasks;

namespace MidiService.Protocol.Serialization
{
    public class MidiStreamSerializer
    {
        //private BufferedStream _buffer;
        private Stream _stream;


        public MidiStreamSerializer(Stream stream)
        {
            _stream = stream; 
            //_buffer = new BufferedStream(stream);

        }



        public void Serialize<T>(T message) where T : ProtocolMessage
        {
            ProtoBuf.Serializer.SerializeWithLengthPrefix<T>(_stream, message, PrefixStyle.Base128);

            _stream.Flush();
        }

        public T Deserialize<T>() where T : ProtocolMessage
        {

            var msg = ProtoBuf.Serializer.DeserializeWithLengthPrefix<T>(_stream, PrefixStyle.Base128);

            return msg;

        }


    }




}
