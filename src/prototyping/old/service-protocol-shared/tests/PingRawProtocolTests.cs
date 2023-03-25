// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol;
using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ProtocolTests
{
    [TestClass]
    public class PingRawProtocolTests
    {
        private Guid _clientId = Guid.NewGuid();
        private Version _clientVersion = new Version(1, 0, 0);



        [TestMethod]
        public void TestPingService()
        {
            var request = new ServicePingMessage()
            {
                Header = new RequestMessageHeader()
                {
                    ClientId = _clientId,
                    ClientRequestId = Guid.NewGuid(),
                    ClientVersion = _clientVersion.ToString(),
                },
            };

            DisplayPingMessage(request);

            using (NamedPipeClientStream sessionPipe = new NamedPipeClientStream(
                ".", // local machine
                MidiServiceConstants.PingPipeName,
                PipeDirection.InOut, PipeOptions.None
                ))
            {
                // get the serializer ready
                MidiServiceProtocolSerializer serializer = new MidiServiceProtocolSerializer(sessionPipe);

                // connect to pipe. This could take a while if pipe is busy
                sessionPipe.Connect();

                // send request message
                serializer.Serialize(request);

                // wait for response (reading blocks until data arrives)
                ServicePingResponseMessage response = serializer.Deserialize<ServicePingResponseMessage>();

                DisplayPingResponseMessage(response);

                // make sure we're getting our own correct response from this request
                Assert.AreEqual(request.Header.ClientRequestId, response.Header.ClientRequestId, "Client request Ids do not match.");
            }

        }




        private void DisplayPingMessage(ServicePingMessage msg)
        {
            Console.WriteLine("Ping request");
            Console.WriteLine(" + Header ---------- ");
            Console.WriteLine(" - Client Id:        " + msg.Header.ClientId);
            Console.WriteLine(" - Request Id:       " + msg.Header.ClientRequestId);
            Console.WriteLine(" - Client Version:   " + msg.Header.ClientVersion);
        }

        private void DisplayPingResponseMessage(ServicePingResponseMessage msg)
        {
            Console.WriteLine("Ping response");
            Console.WriteLine(" + Header ---------- ");
            Console.WriteLine(" - Response Code:    " + msg.Header.ResponseCode);
            Console.WriteLine(" - Client Id:        " + msg.Header.ClientId);
            Console.WriteLine(" - Request Id:       " + msg.Header.ClientRequestId);
            Console.WriteLine(" - Server Version:   " + msg.Header.ServerVersion);
        }

    }
}
