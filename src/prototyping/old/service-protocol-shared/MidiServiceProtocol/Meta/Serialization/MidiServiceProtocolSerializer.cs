// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml.Serialization;
using System.Threading.Tasks;
using System.Text.Json;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Base;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Serialization
{
    // Uses Google Protobuf.
    // Note that I tried the JsonSerializer as well. But that doesn't work well
    // with Pipe streams unless you handle message framing manually
    // like adding a header with a count of message bytes to follow and then
    // manually parse the stream. May need to revisit that anyway for pipes
    // which can have many types of messages. The thing I do not like about
    // protobuf-net is the markup on types, and having to carry that around
    // everywhere. Could use .proto files, but that's yet another place where
    // type information gets duplicated and which can get out of sync.
    // Could also consider the Xml Serializer. Huge messages, but also 
    // somewhat more flexible.
    // Also MessagePack: https://github.com/neuecc/MessagePack-CSharp#comparison
    // but it has some of the same limitations as ProtoBuf-net
    public class MidiServiceProtocolSerializer
    {
        //private BufferedStream _buffer;
        private Stream _stream;
        //private BufferedStream _buffer;

        //private JsonSerializerOptions _options;

        public MidiServiceProtocolSerializer(Stream stream)
        {
            _stream = stream; 
           // _buffer = new BufferedStream(stream);
           // _options = new JsonSerializerOptions() { IncludeFields = true };
        }

        public void Serialize<T>(T message) where T : ProtocolMessage
        {
            ProtoBuf.Serializer.SerializeWithLengthPrefix<T>(_stream, message, PrefixStyle.Base128);
            _stream.Flush();

            //JsonSerializer.Serialize<T>(_buffer, message);
            //_buffer.Flush();
        }

        public T Deserialize<T>() where T : ProtocolMessage
        {
            var msg = ProtoBuf.Serializer.DeserializeWithLengthPrefix<T>(_stream, PrefixStyle.Base128);

            // NOTE: This reads the stream until the end. If there are multiple
            // messages, this may be a problem
            //var msg = JsonSerializer.Deserialize<T>(_stream);

            return msg;
        }


    }




}
