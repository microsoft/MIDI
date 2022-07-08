# Routing plug-ins

Plug-ins for message routing. These need access to more data (endpoints, specifically) than processing plug-ins, so they will likely be a separate plug-in interface. TBD.

Examples of routing plug-ins

* Treating a group of MIDI endpoints or channels connected to monosynths as a single virtual polysynth. There are dedicated boxes which can do this today, but nothing that is in software and automatically available to all apps using the MIDI API.
* Message replication to multiple MIDI devices without the software needing to know about this
* Software-controlled layers and split points