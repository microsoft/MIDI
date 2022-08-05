
using System;
using System.Collections.Generic;
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System.Diagnostics;
using System.IO.Pipes;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Windows.Midi.Enumeration;
using Microsoft.Windows.Midi.Internal.Data;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Serialization;
using Windows.Foundation.Metadata;

namespace Microsoft.Windows.Midi.Session
{

    public sealed partial class MidiSession : IDisposable
    {
        private MidiSessionSerializableData _data;
        private bool disposedValue;

        public Guid Id => _data.Id; 

        // TODO: Updating name here needs to change it on the server
        public string Name 
        { get => _data.Name; set => _data.Name = value; }
        
        public DateTime CreatedTime { get => _data.CreatedTime; internal set => _data.CreatedTime = value; }


        // ================================================================================
        #region Session lifetime

        public static MidiSession Create(string name, MidiSessionCreateOptions options)
        {
            MidiSessionSerializableData data = new MidiSessionSerializableData();

            data.ClientId = ClientState.ClientId;
            data.ProcessName = Process.GetCurrentProcess().ProcessName;
            data.ProcessId = Process.GetCurrentProcess().Id;

            // Send connection request

            var request = new CreateSessionRequestMessage()
            {
                Header = new RequestMessageHeader()
                {
                    ClientId = data.ClientId,
                    ClientRequestId = Guid.NewGuid(),
                    ClientVersion = ClientState.ApiVersion.ToString(),
                },

                Name = name,
                ProcessName = data.ProcessName,
                ProcessId = data.ProcessId 
            };

            using (NamedPipeClientStream sessionPipe = new NamedPipeClientStream(
                ".", // local machine
                MidiServiceConstants.InitialConnectionPipeName,
                PipeDirection.InOut, PipeOptions.None
                ))
            {
                // get the serializer ready
                var serializer = new MidiServiceProtocolSerializer(sessionPipe);

                // connect to pipe. This could take a while if pipe is busy
                sessionPipe.Connect();

                // send request message. This happens automatically because Serialize writes to stream
                serializer.Serialize(request);

                // wait for response (reading blocks until data arrives)
                var response = serializer.Deserialize<CreateSessionResponseMessage>();

                // TODO: Check ClientRequestId values match

                if (response.Header.ClientRequestId == request.Header.ClientRequestId)
                {
                    data.CreatedTime = response.CreatedTime;
                    data.Id = response.NewSessionId;

                    MidiSession session = new MidiSession()
                    {
                        _data = data,
                    };



                    // TODO: Open the session-specific pipe 

                    if (!options.SkipDeviceEnumeration)
                    {
                        // TODO: enumerate devices and endpoints using the static enumerator
             //           MidiEnumerator.GetDevices();
             //           MidiEnumerator.GetEndpoints();
                    }


                    return session;

                }
                else
                {
                    // Should throw an exception. We're somehow replying to someone else's message
                }

            }

            return null;

        }

        public static MidiSession Create(string name)
        {
            return Create(name, new MidiSessionCreateOptions());
        }

        public void Close()
        {
            // TODO: Tell server we're done and to remove the session. There's no response for this message
        }

        // ================================================================================
        #region IDisposable
        private void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // TODO: dispose managed state (managed objects)
                }

                Close();

                // TODO: set large fields to null
                disposedValue = true;
            }
        }

        // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        ~MidiSession()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: false);
        }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        #endregion







        #endregion




        // ================================================================================
        // ================================================================================
        // ================================================================================



        // todo: Method to save local property changes to server
        // Do this instead of per-property to make the change
        // use as few messages as possible

        // TODO: Device and endpoint lifetime / CRUD


        // TODO: Utility functions




    }
}
