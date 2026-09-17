// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.Devices.Midi2.Utilities.Sequencing;

namespace WindowsMidiServices
{
    // Handed back by Start-MidiFilePlayback -NoWait. The player owns a connection and the
    // connection belongs to a session, so both have to be held for as long as the file is
    // playing. Without something for the script to keep in a variable, the collector would take
    // them and playback would stop for no visible reason.
    public class MidiFilePlayback : IDisposable
    {
        private readonly object _lock = new();

        public string FilePath { get; }

        public string EndpointDeviceId { get; }

        public bool IsPlaying
        {
            get
            {
                lock (_lock)
                {
                    return BackingPlayer is not null && BackingPlayer.State == MidiSequencePlayerState.Playing;
                }
            }
        }

        public MidiSequencePlayerState State
        {
            get
            {
                lock (_lock)
                {
                    return BackingPlayer is null ? MidiSequencePlayerState.Stopped : BackingPlayer.State;
                }
            }
        }

        internal MidiSequencePlayer? BackingPlayer { get; set; }

        internal Windows.Devices.Midi2.MidiSession? BackingSession { get; set; }

        public MidiFilePlayback(
            MidiSequencePlayer player,
            Windows.Devices.Midi2.MidiSession session,
            string filePath,
            string endpointDeviceId)
        {
            BackingPlayer = player;
            BackingSession = session;
            FilePath = filePath;
            EndpointDeviceId = endpointDeviceId;
        }

        // Stopping silences the instrument first. A player which is simply dropped leaves the
        // last chord sounding until something else turns it off.
        internal void StopAndRelease()
        {
            lock (_lock)
            {
                if (BackingPlayer is not null)
                {
                    BackingPlayer.Stop();
                    BackingPlayer.Dispose();
                    BackingPlayer = null;
                }

                if (BackingSession is not null)
                {
                    BackingSession.Dispose();
                    BackingSession = null;
                }
            }
        }

        public void Dispose()
        {
            StopAndRelease();

            GC.SuppressFinalize(this);
        }

        ~MidiFilePlayback()
        {
            StopAndRelease();
        }
    }
}
