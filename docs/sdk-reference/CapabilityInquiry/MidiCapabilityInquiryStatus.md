---
layout: sdk_reference_page
title: MidiCapabilityInquiryStatus
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: enum
description: How a capability inquiry request ended
---

A device is allowed to ignore a request it does not implement, so silence is an ordinary outcome and not a fault in the transport.

| Value | Number | Description |
| ----- | ------ | ----------- |
| `Success` | 0 | The device answered, and the answer is in the response |
| `NoResponse` | 1 | Nothing came back before the timeout. Usually means the device does not implement the request, but a busy device or a crowded cable can also produce it |
| `NegativeAcknowledgment` | 2 | The device answered and said no. The status code and the device's own explanation say why, and the specification asks that the explanation be shown to the person using the application |
| `InvalidResponse` | 3 | Something came back which could not be read as an answer to the request |
| `NotSupported` | 4 | The device declared in Discovery that it does not do this at all, so nothing was sent |
| `Canceled` | 5 | The request was abandoned before it finished |
| `Failed` | 6 | The request could not be sent. The session has no connection, or the connection refused the messages |
