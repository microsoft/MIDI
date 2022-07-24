# API Specifications: MIDI CI

=======================================================
TODO: This needs to be updated after the SDK changes
=======================================================

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

## MIDI-CI

TODO: These specs are not accurate as they do not take into account groups for CI negotiation

### Discovery and Protocol Negotiation

Endpoint instances have a user-settable flag to enable/disable protocol negotiation. This is there in case the user knows the endpoint will never support MIDI 2.0, or in case protocol negotiation somehow breaks an existing device, or takes too long to start up, or just in case the user only wants that device to be visible to Windows as a MIDI 1.0 device.

MIDI CI discovery and protocol negotiation will only be attempted on Bi-directional endpoints.

Authority level of MIDI CI messages initiatied by the API/driver shall default to the highest level (0x60-0x6F)
* This should be end-user configurable in the settings app, just in case

TODO: Decide when all this happens. For the user, on startup could be ideal, except that could greatly delay startup, which is against Windows implementation guidelines. It could happen on the first time any app uses the MIDI API, but that will create a delay at that point. It could also be something which happens as a delayed startup in Windows, but that's not ideal in case the user goes right into their DAW.

Discovery and protocol negotiation are implemented internally, so need no API other than exposing which protocol was negotiated

```cpp

// no application-level APIs for discovery or protocol negotiation
// however, properties are exposed
if (_endpoint.ProtocolType == 1)
    // MIDI 1.0
else if (_endpoint.ProtocolType == 2)
    // MIDI 2.0

```

There are other properties set by CI which will be useful for the endpoints. It may be that the endpoint itself contains only the raw capability inquiry bitmap, and that the flags are interpreted by a helper class, like what we're doing for message parsing. This keeps the core API lean.

```cpp
if (_endpoint.ProtocolNegotiationSupported)     // useful only for debugging.
if (_endpoint.ProfileConfigurationSupported)    // will be used by apps. Set by discovery/capability inquiry.
if (_endpoint.PropertyExchangeSupported)        // will be used by apps. Set by discovery/capability inquiry.

_endpoint.RawCapabilityInquiryResult            // the raw result
```

### Property Exchange

We won't store or otherwise deal with properties in the OS, but we can provide code to handle the transaction. We may also provide helper classes to parse property information. TBD.

TODO: Also need to provide a way to deal with subscriptions to property changes through MIDI CI 8.12

### Profiles

We won't store or otherwise deal with profiles in the OS, but we can provide code to handle the transaction. We may also provide helper classes to parse the results. TBD.

### Other messages not handled within the API

These are messages which will be handled by the code interfacing with the API (DAW, plugin, etc.)

(properties etc.)

