---
layout: sdk_reference_page
title: MidiPropertySubscription
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A standing request to be told when a resource on a device changes
---

Asking a device for a resource over and over takes a full round trip every time, and it still misses changes between requests. A subscription works the other way around: the device tells you when the resource changes, and sends you the new value along with the news.

Not every resource can be subscribed to, and the device decides which ones can. Read its [`MidiResourceList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceList) first, and check `CanSubscribe` on the entry you care about. If you ask for one the device doesn't offer, the device says no instead of ignoring you, so it's safe to try. But reading the list first saves a round trip.

`ChannelList` is the resource most worth subscribing to. It says what's selected on each channel right now, which is exactly what changes while someone is playing.

A subscription belongs to the session that created it. Closing the session ends every subscription it holds.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | How the request to subscribe ended. `Success` means the device agreed |
| `ResourceStatus` | The status the device put in its reply header. 200 means it agreed. 405 is the usual refusal, and means the device doesn't allow subscriptions to that resource |
| `ResponderMuid` | The device that holds the subscription |
| `Resource` | What was subscribed to |
| `ResourceId` | Which copy of the resource, or empty for a resource the device publishes only once |
| `SubscribeId` | The identifier the device gave the subscription. Every update the device sends includes it, and that's how an update is matched to its subscription |
| `IsActive` | False once the subscription has ended, whether your application ended it or the device did. You can't restart a subscription that has ended. Ask for a new one |

## Examples

### Subscribe to a device's channel list

```cpp
auto const resources = session.GetResourceListAsync(muid).get();
auto const entry = resources.GetEntry(L"ChannelList");

if (entry != nullptr && entry.CanSubscribe())
{
    session.PropertySubscriptionUpdated([](auto&&, auto const& args)
        {
            // args.Update().BodyAsJson() is the new channel list
        });

    auto const subscription = session.SubscribeAsync(muid, L"ChannelList", L"").get();

    if (!subscription.IsActive())
    {
        // the device refused. ResourceStatus says why
    }
}
```
