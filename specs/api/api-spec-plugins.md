# API Specifications: Plug-ins

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

## Plug-in Base

TODO

## Device Plug-ins

See devices and transports API spec

## Processing Plug-ins

TODO: Plug-ins for message processing. Also not fully thought through yet.

```cpp

// for processing a single message. We may need to provide
// an interface which supports processing multiple messages
// or a stream, or supports transforming the number of
// messages so that it is more or less than what was passed
// in. Those may require just passing in a stream reference
// instead, which has some threading implications
interface IMidiIncomingMessageProcessor
{
    // These return true if the message should be passed along
    // and false if they should be removed from the stream
    bool ProcessMessage(Ump32* messageToProcess)
    bool ProcessMessage(Ump64* messageToProcess)
    bool ProcessMessage(Ump96* messageToProcess)
    bool ProcessMessage(Ump128* messageToProcess)
}

interface IMidiOutgoingMessageProcessor
{
    // These return true if the message should be passed along
    // and false if they should be removed from the stream
    bool ProcessMessage(Ump32* messageToProcess)
    bool ProcessMessage(Ump64* messageToProcess)
    bool ProcessMessage(Ump96* messageToProcess)
    bool ProcessMessage(Ump128* messageToProcess)
}

```

Possible incoming messages workflow:

* Driver supplies stream to service
* Service gets stream ready to send to each listening app
  * If there are incoming message processors attached to incoming endpoint, run them in order
  * It may be necessary, if messages are removed/added, to have a second "post-process" stream which is then sent to apps
  * If there are no message processors, can maybe optimize to have only a single stream shared with apps in that case.
  * The above really depends on performance of creating a postprocess stream in all cases vs just when there are processors

Possible outgoiing messages workflow:

* App writes UMPs to stream
* Service gets stream ready to send to endpoints
  * If there are any outgoing message processors attached to output endpoint, run them in order
  * Same caveats about stream copies as above
* Service sends stream to endpoints

TBD: Who is responsible for adding anti-jitter timestamps? Presumably the service will need to do this, but that requires inserting a number of messages into a copy of the stream

TBD: This model won't support time-based continuous processing, like an arpeggiator MIDI effect and similar. The overhead of something like that as a plug-in may be too high. It may be better as an app on a virtual endpoint, or simply left to dedicted DAWs and performance effects.

## Routing Plug-ins

TBD. It may be more scalable to handle this through virtual MIDI endpoints. That's how I'm currently thinking of this.
