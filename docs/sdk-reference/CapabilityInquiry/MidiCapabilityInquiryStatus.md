---
layout: sdk_reference_page
title: MidiCapabilityInquiryStatus
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: enum
description: How a capability inquiry request ended
---

A device is allowed to ignore a request it doesn't support. So getting no answer is normal, and it doesn't mean something is broken.

| Value | Number | Description |
| ----- | ------ | ----------- |
| `Success` | 0 | The device answered, and the answer is in the response |
| `NoResponse` | 1 | Nothing came back before the time ran out. Usually this means the device doesn't support the request, but a busy device or a busy connection can also cause it |
| `NegativeAcknowledgment` | 2 | The device answered and said no. The status code and the device's own message say why. The specification asks that you show that message to the person using your application |
| `InvalidResponse` | 3 | Something came back, but it couldn't be read as an answer to the request |
| `NotSupported` | 4 | The device said in Discovery that it doesn't do this at all, so nothing was sent |
| `Canceled` | 5 | The request was stopped before it finished |
| `Failed` | 6 | The request couldn't be sent. The session has no connection, or the connection refused the messages |
