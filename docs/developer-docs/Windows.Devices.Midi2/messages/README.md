# Messages

MIDI Messages are discrete packets of data of a known length. In the MIDI 2.0 specification, they are known as Universal MIDI Packets. In Windows MIDI Services, even MIDI 1.0 bytestream messages are presented in their equivalent Universal MIDI Packet format. The API includes several classes not only for the messages, but also to help construct and parse them.

* [Midi Message Types](./midi-message-types/)
* [MidiMessageBuilder](./MidiMessageBuilder/)
* [MidiMessageConverter](./MidiMessageConverter/)
* [MidiMessageTranslator](./MidiMessageTranslator/)
* [MidiMessageUtility](./MidiMessageUtility/)
* [MidiStreamMessageBuilder](./MidiStreamMessageBuilder/)