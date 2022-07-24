# Routing plug-ins

Plug-ins for message routing. These need access to more data (endpoints, specifically) than processing plug-ins, so they will likely be a separate plug-in interface. 

TBD. It is likely that we'll simply bake this type of functionality into the virtual MIDI devices and endpoints instead of a plug-in approach

Examples of routing plug-ins

* Treating a group of MIDI endpoints or channels connected to monosynths as a single virtual polysynth. There are dedicated boxes which can do this today, but nothing that is in software and automatically available to all apps using the MIDI API.
* Message replication to multiple MIDI devices without the software needing to know about this
* Software-controlled layers and split points, and otherwise sending different note ranges to different endpoints
