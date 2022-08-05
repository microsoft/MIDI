// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Session;
using System.Diagnostics;
using System.IO.Pipes;
using System.Runtime.Serialization;


namespace ServiceTests
{
    [TestClass]
    public class SessionRawProtocolTests
    {
        private Guid _clientId = Guid.NewGuid();
        private Version _clientVersion = new Version(1, 0, 0);

        [TestMethod]
        public void TestOpeningConnectionPipe()
        {
            using (NamedPipeClientStream sessionPipe = new NamedPipeClientStream(
                ".", // local machine
                MidiServiceConstants.InitialConnectionPipeName,
                PipeDirection.InOut, PipeOptions.None
                ))
            {
                // nothing to do if no exception
                Assert.IsNotNull(sessionPipe);
            }
        }

        [TestMethod]
        public void TestCreateSession()
        {

            // Open main communications pipe

            // Send connection request

            var request = new CreateSessionRequestMessage()
            {
                Header = new RequestMessageHeader()
                {
                    ClientId = _clientId,
                    ClientRequestId = Guid.NewGuid(),
                    ClientVersion = _clientVersion.ToString(),
                },

                Name = "Test Session",
                //MachineName = Process.GetCurrentProcess().MachineName,
                ProcessName = Process.GetCurrentProcess().ProcessName,
                ProcessId = Process.GetCurrentProcess().Id
            };

            DisplaySessionCreateRequestMessage(request);

            using (NamedPipeClientStream sessionPipe = new NamedPipeClientStream(
                ".", // local machine
                MidiServiceConstants.InitialConnectionPipeName,
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
                CreateSessionResponseMessage response = serializer.Deserialize<CreateSessionResponseMessage>();

                DisplaySessionCreateResponseMessage(response);

                // make sure we're getting our own correct response from this request
                Assert.AreEqual(request.Header.ClientRequestId, response.Header.ClientRequestId, "Client request Ids do not match.");
            }

            // Test Opening the session-specific pipe

        }

        private void DisplaySessionCreateRequestMessage(CreateSessionRequestMessage msg)
        {
            Console.WriteLine("Session Connection request");
            Console.WriteLine(" + Header ---------- ");
            Console.WriteLine(" - Client Id:        " + msg.Header.ClientId);
            Console.WriteLine(" - Request Id:       " + msg.Header.ClientRequestId);
            Console.WriteLine(" - Client Version:   " + msg.Header.ClientVersion);
            Console.WriteLine(" + Session --------- ");
            Console.WriteLine(" - Session Name:     " + msg.Name);
            Console.WriteLine(" - Process Name:     " + msg.ProcessName);
            Console.WriteLine(" - Process Id:       " + msg.ProcessId);
        }

        private void DisplaySessionCreateResponseMessage(CreateSessionResponseMessage msg)
        {
            Console.WriteLine("Session Connection response");
            Console.WriteLine(" + Header ---------- ");
            //Console.WriteLine(" - Opcode:           " + msg.Opcode);
            Console.WriteLine(" - Response Code:    " + msg.Header.ResponseCode);
            Console.WriteLine(" - Client Id:        " + msg.Header.ClientId);
            Console.WriteLine(" - Request Id:       " + msg.Header.ClientRequestId);
            Console.WriteLine(" - Server Version:   " + msg.Header.ServerVersion);
            Console.WriteLine(" + Session --------- ");
            Console.WriteLine(" - New Session Id:   " + msg.NewSessionId);
            Console.WriteLine(" - Session Pipe:     " + msg.SessionChannelName);
            Console.WriteLine(" - Created At:       " + msg.CreatedTime);
        }




    }
}