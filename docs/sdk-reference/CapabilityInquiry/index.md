---
layout: sdk_namespace_page
title: MIDI CI Namespace Overview
namespace: Windows.Devices.Midi2.CapabilityInquiry
description: Namespace for MIDI-CI types
---

MIDI Capability Inquiry is how a device tells you what it is, what its channels are set to, and what patches it has, without your application needing to know anything about that make of device. A patch browser in a sequencer is built on it.

Most applications want [`MidiCapabilityInquirySession`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquirySession). It owns an identifier, numbers its own requests, puts chunked replies back together, pages through long lists and gives up on a device that is not going to answer, so what is left in your code is the question you wanted to ask.

An application that wants to look like a device instead uses `MidiVirtualDevice.CapabilityInquiry`, a [`MidiCapabilityInquiryDeviceResponder`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryDeviceResponder). Hand it the lists you want to publish and it answers on your behalf.

Below both of those sit [`MidiCapabilityInquiryMessage`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryMessage) and [`MidiCapabilityInquiryMessageBuilder`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryMessageBuilder), which read and write individual messages. Use those to do something neither of the two higher level types covers.

Everything here travels as UMP. There is no bytestream path.

## Sessions and devices

| Type | Description |
| ---- | ----------- |
| [`MidiCapabilityInquirySession`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquirySession) | Asks capability inquiry questions over an endpoint connection and waits for the answers |
| [`MidiCapabilityInquiryDeviceResponder`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryDeviceResponder) | Answers capability inquiry on a virtual device's behalf |
| [`MidiCapabilityInquiryResponder`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryResponder) | A device which answered Discovery |
| [`MidiPropertyExchangeResponse`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiPropertyExchangeResponse) | The answer to a property exchange request |
| [`MidiProfileInquiryResponse`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiProfileInquiryResponse) | What a responder said about the profiles at one address |
| [`MidiCapabilityInquiryMessageReceivedEventArgs`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryMessageReceivedEventArgs) | A message which arrived without having been asked for |

## Messages

| Type | Description |
| ---- | ----------- |
| [`MidiCapabilityInquiryMessage`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryMessage) | A message taken off the wire and decoded |
| [`MidiCapabilityInquiryMessageBuilder`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryMessageBuilder) | Builds messages as the UMP packets which carry them |
| [`MidiCapabilityInquiryMessageType`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryMessageType) | Which message a message is |
| [`MidiCapabilityInquiryCategories`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryCategories) | What a device says it can do |
| [`MidiCapabilityInquiryStatus`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryStatus) | How a request ended |
| [`MidiUniqueId`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiUniqueId) | The identifier a device answers to, for as long as it is powered on |
| [`MidiProfileId`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiProfileId) | The five byte identifier of a profile |

## Property exchange resources

| Type | Description |
| ---- | ----------- |
| [`MidiDeviceInfo`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiDeviceInfo) | What a device says about itself |
| [`MidiResourceList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceList) | Everything a device offers, and what may be done with each |
| [`MidiResourceListEntry`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceListEntry) | One entry in that list |
| [`MidiChannelList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiChannelList) | What a device says about each of its channels |
| [`MidiChannelListEntry`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiChannelListEntry) | One entry in that list |
| [`MidiProgramList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiProgramList) | The programs a device offers |
| [`MidiProgramListEntry`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiProgramListEntry) | One entry in that list |
| [`MidiResourceLink`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceLink) | A pointer from one resource to another |

## Examples

* [C++ Sample: browsing a device](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/capability-inquiry-browse/main_capability_inquiry_browse.cpp)
* [C++ Sample: being a device](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/capability-inquiry-virtual-device/main_capability_inquiry_virtual_device.cpp)
* [C# Sample: browsing a device](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/capability-inquiry-browse/Program.cs)
* [C# Sample: being a device](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/capability-inquiry-virtual-device/Program.cs)
