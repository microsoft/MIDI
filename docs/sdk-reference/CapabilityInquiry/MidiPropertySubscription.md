---
layout: sdk_reference_page
title: MidiPropertySubscription
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A standing request to be told when a resource on a device changes
---

Polling a device for a resource costs a full round trip every time, and it still misses changes between polls. A subscription turns that around: the device tells you when the resource changed, and sends you the new value with the news.

Not every resource can be subscribed to, and it is the device that decides. Read its [`MidiResourceList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceList) first and check `CanSubscribe` on the entry you care about. Asking for one the device does not offer is answered with a refusal rather than silence, so it is safe to try, but reading the list first saves a round trip.

`ChannelList` is the resource most worth subscribing to. It says what is selected on each channel right now, which is exactly the thing that changes while someone is working.

A subscription belongs to the session that created it. Closing the session ends every subscription it holds.

## Properties

| Property | Description |
| -------- | ----------- |
| `Status` | How the request to subscribe ended. `Success` means the device accepted |
| `ResourceStatus` | The status the device put in its reply header. 200 means it accepted. 405 is the usual refusal, and means the device does not allow subscriptions to that resource |
| `ResponderMuid` | The device holding the subscription |
| `Resource` | What was subscribed to |
| `ResourceId` | Which instance of it, or empty for a resource the device publishes once |
| `SubscribeId` | The identifier the device assigned. Every update it sends carries this, and it is how an update is matched back to the subscription that asked for it |
| `IsActive` | False once the subscription has ended, whether the application ended it or the device did. An ended subscription cannot be restarted; ask for a new one |

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
