// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.CapabilityInquiry;

namespace WindowsMidiServices
{
    // Asks a device, over MIDI-CI Property Exchange, for the programs (patches) it can play, with
    // the bank and program change that selects each one. A long list arrives a page at a time, and
    // what is returned here is the whole of it.
    [Cmdlet(VerbsCommon.Get, "MidiProgramList", DefaultParameterSetName = EndpointDeviceIdParameterSet)]
    [OutputType(typeof(MidiProgramListEntry))]
    public class CommandGetMidiProgramList : MidiCapabilityInquiryCmdletBase
    {
        // One list, by the id the device gave it. Without this, every list the device's channels
        // point at is fetched, which is the only way to reach a device's second collection.
        [Parameter]
        public string? ResourceId { get; set; }

        protected override void ProcessRecord()
        {
            QueryPropertyExchangeResponders((session, responder) =>
            {
                if (ResourceId is not null)
                {
                    EmitProgramList(session, responder, ResourceId, null);
                    return;
                }

                // A device which publishes no resource list is not saying it has nothing. Only a
                // list which came back can rule a resource out.
                var resources = session.GetResourceListAsync(responder.Muid).GetAwaiter().GetResult();

                var offersChannelList = true;

                if (resources is not null && resources.Entries.Count > 0)
                {
                    if (!resources.SupportsResource(ProgramListResourceName))
                    {
                        WriteNonTerminating(
                            new NotSupportedException(Format(Strings.CiProgramListNotOfferedFormat, responder.Muid)),
                            "MidiProgramListNotOffered",
                            ErrorCategory.NotImplemented,
                            QueriedEndpointDeviceId);

                        return;
                    }

                    offersChannelList = resources.SupportsResource(ChannelListResourceName);
                }

                IList<MidiResourceLink> links = [];

                if (offersChannelList)
                {
                    var channels = session.GetChannelListAsync(responder.Muid).GetAwaiter().GetResult();

                    if (channels is not null)
                    {
                        links = channels.GetProgramListLinks();
                    }
                }

                if (links.Count == 0)
                {
                    // A device with one list does not need its channels to point at it.
                    EmitProgramList(session, responder, string.Empty, null);
                    return;
                }

                // Labeled only when there is more than one, because a single collection's title
                // tells the caller nothing.
                foreach (var link in links)
                {
                    var collectionTitle = links.Count < 2
                        ? null
                        : (string.IsNullOrEmpty(link.Title) ? link.ResourceId : link.Title);

                    EmitProgramList(session, responder, link.ResourceId, collectionTitle);
                }
            });
        }

        private void EmitProgramList(
            MidiCapabilityInquirySession session,
            MidiCapabilityInquiryResponder responder,
            string resourceId,
            string? collectionTitle)
        {
            var programs = session.GetProgramListAsync(responder.Muid, resourceId).GetAwaiter().GetResult();

            if (programs is null)
            {
                var message = string.IsNullOrEmpty(resourceId)
                    ? Format(Strings.CiProgramListNotSentFormat, responder.Muid)
                    : Format(Strings.CiNamedProgramListNotSentFormat, responder.Muid, resourceId);

                WriteNonTerminating(
                    new InvalidOperationException(message),
                    "MidiProgramListUnavailable",
                    ErrorCategory.ResourceUnavailable,
                    QueriedEndpointDeviceId);

                return;
            }

            WriteVerbose(Format(Strings.CiProgramsReceivedFormat, programs.Entries.Count));

            foreach (var entry in programs.Entries)
            {
                if (collectionTitle is not null && string.IsNullOrEmpty(entry.CollectionTitle))
                {
                    entry.CollectionTitle = collectionTitle;
                }

                WriteObject(entry);
            }
        }
    }
}
