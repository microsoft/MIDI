// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


namespace Microsoft.Midi.ConsoleApp
{
    // might be good if this could also be an index into a list we display
    // could make it optional, and then if missing, use AnsiConsole.Prompt()
    internal class EndpointCommandSettings : CommandSettings
    {
        [LocalizedDescription("ParameterCommonEndpointIdDescription")]
        [CommandArgument(0, "[Endpoint Device Id]")]
        public string? EndpointDeviceId { get; set; }
    }

    internal class SendMessageCommandSettings : EndpointCommandSettings
    {
        [LocalizedDescription("ParameterSendMessageDelayBetweenMessages")]
        [CommandOption("-p|--pause|--delay")]
        [DefaultValue(2)]
        public int DelayBetweenMessages { get; set; }

        [EnumLocalizedDescription("ParameterSendMessageWordFormat", typeof(MidiWordDataFormat))]
        [CommandOption("-w|--word-format")]
        [DefaultValue(MidiWordDataFormat.Hex)]
        public MidiWordDataFormat WordDataFormat { get; set; }

        [LocalizedDescription("ParameterSendMessageNoWait")]
        [CommandOption("-n|--no-wait")]
        [DefaultValue(false)]
        public bool NoWait { get; set; }

        //[LocalizedDescription("TODO ParameterSendMessageAutoNegotiation")]
        //[CommandOption("-n|--auto-negotiation")]
        //[DefaultValue(true)]
        //public bool AutoProtocolNegotiation { get; set; }

        //[LocalizedDescription("TODO ParameterSendMessageAutoDiscovery")]
        //[CommandOption("-y|--auto-discovery")]
        //[DefaultValue(true)]
        //public bool AutoDiscovery { get; set; }

    }

    internal class MessageListenerCommandSettings : EndpointCommandSettings
    {
        //[EnumLocalizedDescription("ParameterListenerMessagesFilter", typeof(ListenerMessageTypeFilter))]
        //[CommandOption("-f|--filter")]
        //[DefaultValue(ListenerMessageTypeFilter.All)]
        //public ListenerMessageTypeFilter Filter { get; set; }
    }

    internal class EndpointRequestCommandSettings : EndpointCommandSettings
    {
    }
}
