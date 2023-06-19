# SDK

| Project | Description |
| -------- | --------------- |
| sdk | SDK for apps to use to work with Windows MIDI Services |
| sdk-projections | Any required projections for languages other than C++ |


The main SDK is likely to be broken up into a few projects to ensure developers only need to ship what they will use. 

* Core
* Strongly-typed UMP messages
* Possible UI controls
* Diagnostics
* Profiles and Properties
* etc.

Note: this SDK shaping is far from complete. The main item that is going to change here is the most critical part: sending and receiving of messages. That requires close integration with the core API and service to ensure it performs well and minimizes memory copies.

Additionally, the namespace setup is very flat at the moment. This will change.