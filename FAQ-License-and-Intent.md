# FAQ for license and forking

## Philosophy - why is this Open Source?

We believe in the musician and music technology communities. We also know that the music creation tech community is a highly motivated and interested community, including those who are both musicians and developers who are working to move music technology forward.

We recognize that standards like MIDI 2.0 are living standards, with new transports, messages, and add-on capabilities added over time. In the past, we haven't been quick to adopt those in releases of Windows, despite a more agile approach to Windows development. We want to change that.

We proposed that the best way to maintain an API which keeps up with the evolving MIDI standards and offers early adopters in this community an opportunity to try them out, test, and contribute, is to open source everything that we can, and invite both internal teams and the community to contribute to MIDI. **We also love the idea of others learning from this source code to implement MIDI 2.0 on embedded devices and other operating systems as they need.**

In short, we believe this project is the best way to continue to develop MIDI on Windows in the long-term, and to give back to the whole MIDI developer community, regardless of which operating system they prefer.

> There are some pieces which require changes to core Windows for us to implement. For example, we have made some changes to our USB stack to enable detection of the new class driver without interfering with USB Audio 1.0 (USB Audio 1.0 and MIDI 1.0 are in the same driver, but in the new model, MIDI 1.0 and MIDI 2.0 are in the new driver, but Audio 1.0 stays in the old driver). We're also porting some driver model work, and may have some other shims which redirect MIDI 1.0 API calls to the new stack. Most or all of these pieces will not be open source because they are based on existing private Windows code. But, wherever we can be open, we are and will be. Our default approach for this project is open source.

It can be easy for a developer to find an open source project, and going strictly by what's allowed by the license, run afoul of the intentions of the project and the community that supports and contributes to it. I've seen this on other third-party products, mostly around forking the whole repo, or creating plugins that they want to sell instead of contribute.

In general, we won't prevent anything that the license allows, but we do have some preferences. To avoid confusion, here are some common questions to clarify above and beyond what the license says.

## Questions

### Forking vs Contributing

**Q: Can I fork this project and create a version for another operating system?**

A: Yes, for sure. That's part of why we made this open source. Although we'd also encourage you to work with the developers for that operating system's MIDI 2.0 implementation, if available. For example, on Linux, Takashi and team working on the MIDI 2.0 implementation for ALSA.

**Q: Can I fork this project and create a version for a microcontroller or embedded device?**

A: We'd love to see that and perhaps even participate! Note that there are microcontroller implementations already available from AmeNote and the MIDI Association, but they don't cover all devices, of course. Feel free to ask us if we're aware of any, or to check with the MIDI Association, to see if they know of any, as well.

**Q: Can I fork this project and create another version for Windows?**

A: This is, of course, allowed per the license. But we'd prefer you work with us to make this project better for all musicians, rather than produce a competing or overlapping project. This is true whether you're creating a free version or a paid version.

**Q: Can I fork this project and create another version for older Windows versions?**

A: Same answer as above, but with the caveat that technically this will be difficult at best as we're using some OS features (updates to USB stack, driver model, WinRT device information notification, etc) that have been made available only in the versions of Windows we support with this project.

**Q: Can I fork this project and create an SDK that doesn't rely on WinRT?**

A: Please see previous "Can I fork?" answers for overall approach. Also please note that much of the functionality (like device discovery, hot plug/unplug notifications, auto-reconnect, etc.) build upon WinRT components in Windows. We want to have a great experience for all users, so strongly recommend that these features be retained as core SDK features, not optional add-ons, or features not available in a third-party SDK offering.

### Plugins

**Q: Can I create and sell transport or other Windows MIDI Services plugins?**

A: We'd strongly prefer to see all Windows MIDI Services plugins available as free and open source projects. We're not trying to create a third-party market for plugins, but rather just working to make MIDI better for all Windows users. But we do encourage third-party transports (prototypes of upcoming implementations, or implementations of recognized MIDI standards that we have not yet gotten to), or contributions to the first-party transports. As members of the MIDI Association, we'll also want to make sure that we're not implementing something that is counter to the MIDI standards (in progress or accepted). Let's talk!

### Application Development

**Q: Can I sell an application which uses Windows MIDI Services**

A: Yes. Of course.

**Q: Can I create my own different settings app or tool for Windows MIDI Services?**

A: Talk with us first. It may make more sense to participate here as an official contributor, be part of the normal distribution, etc. rather than duplicate efforts.

### Device Development

**Q: Can I use code from this repo in the development of my own commercial device?**

A: We're happy to see you do this as long as you adhere to the terms of the license

### Hosting and Distribution

**Q: Can I host the Windows MIDI Services components on my own site/server and distribute them from there.**

A: Unless you are behind a firewall with no internet access, please point customers to the official distribution. Feel free to talk with us about exceptional circumstances.

**Q: What about signing?**

A: Only the official builds from this repo will be signed and distributed by Microsoft. This is especially important for driver signing.
