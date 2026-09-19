---
layout: kb
title: When saved endpoint settings do not match a device
audience: everyone
description: Why the name, picture and other settings you saved for a MIDI device can stop being applied, why that is usually nothing to worry about, and how to put them back when it is not.
categories:
  - Troubleshooting
---

# When saved endpoint settings do not match a device

Windows MIDI Services lets you give a MIDI device your own name, choose a picture for it, rename its MIDI 1.0 ports and set other options. Those settings are stored and applied whenever the device is there.

Sometimes MIDI Settings will tell you that some of your saved settings do not match anything connected. There are two quite different reasons for that, and it is worth knowing which one you are looking at.

## Most of the time, the device is simply not plugged in

Plenty of people own more instruments and controllers than they can leave connected, and plug in whatever a session needs. If you have saved settings for twelve devices and three are connected, the other nine do not match anything right now. That is entirely normal and nothing is wrong.

Plug the device back in and your settings are applied again immediately, with no action from you. You never need to do anything about these.

## Sometimes the device is there but its identifier changed

The other reason is that the device is connected, but Windows no longer recognizes it as the same one, so your settings are not applied to it. The device works, but it is back to the name it came with.

## Windows cannot tell the two apart, and does not guess

This is the important part. From where Windows is standing, both cases look identical: there are saved settings, and nothing connected matches them. There is no way to know whether a device is in a drawer or sitting on your desk under a new identity.

So both are treated the same way. The saved entry is kept, never deleted, and applied the moment something matches it again. You are told the entries exist, and then left to decide, because you are the only one who knows which of your devices is actually in the room.

The message can be dismissed, and dismissed entries are not raised again. If you keep settings for instruments you use twice a year, say so once and Windows will stop mentioning them.

## Why an identifier changes

Windows MIDI Services stores your settings against the endpoint's identifier, and it deliberately matches on that identifier and nothing else. Matching on softer things, such as the manufacturer name or the USB vendor and product numbers, was tried and removed: several devices can report identical values, and some report the same placeholder serial number as each other, so it applied the settings to the wrong device.

For a USB device, that identifier depends on whether the device reports a serial number.

- **A device that reports a real serial number** keeps the same identity wherever you plug it in. Its settings follow it from port to port.
- **A device that reports no serial number** is identified by the hub and port it is plugged into, because that is the only thing distinguishing it from another one of the same model. Move it and Windows considers it a different device.

Most USB devices do not report a serial number. This is a property of the hardware, not something Windows or the device driver can change.

The same thing happens if you move your configuration file to a new computer, and it can happen after a Windows update if device migration does not complete.

## Putting the settings back

### Using MIDI Settings

Open **MIDI Settings**. If any saved settings match nothing connected, a message appears above the endpoint list. Choose **Review them**.

The left side lists the saved settings which match nothing, shown with the picture you chose, the name you gave them, and a summary of what each one holds. The right side lists the devices those settings could be moved onto.

Windows puts the most likely device at the top and says why, for example that it is the same model and the only one connected. **It does not decide for you.** When nothing is clearly the best match, nothing is selected and the button stays disabled, because guessing would put your settings on the wrong instrument.

Choose the settings you recognize, choose the device, and select **Move it here**. If you no longer want an entry, **Delete it** removes it.

If an entry belongs to a device which is simply not plugged in, leave it alone. Close the dialog and dismiss the message. Moving it onto whatever happens to be connected today would take the settings away from the device they belong to.

The message can be dismissed. Once dismissed, you will not be asked about those same entries again.

### Using the MIDI console

```
midi endpoint customizations list
midi endpoint customizations relink --from <stored id> --to <endpoint device id>
midi endpoint customizations forget --from <stored id>
```

`list` shows every saved customization, whether each one currently matches a device, and what it holds. `relink` moves one onto a different endpoint. `forget` deletes one. Add `--orphaned` to `list` to see only the entries with nothing matching them, which includes both devices you have unplugged and devices whose identifier has changed.

## Entries which hold nothing

Some saved entries contain nothing you would miss, usually because an older version of the settings app wrote an entry when you opened a dialog and pressed Save without changing anything. These are hidden from the review list, and counted separately at the bottom of `midi endpoint customizations list`. They do no harm.

## What is worth most

If you have measured an output latency value for a device, that is the hardest setting to reproduce: it needed a loopback cable and a measurement. Windows records whether a stored latency was measured or typed in, and shows it, so you can tell before you delete something whether you can easily get it back.

## Avoiding it in future

Nothing needs avoiding if you unplug devices between sessions. Saved settings waiting for a device to come back are working exactly as intended, and the only thing you might want to do is dismiss the message once.

For the case where an identifier really has changed: a device that reports a serial number keeps its identity between ports, so this will not happen to it. A device that does not report one keeps its identity only while it stays in the same USB port, or the same port on the same hub, so remembering where those devices are normally plugged in can help reduce the chances of this happening. These tools will help you when that's not a reasonable option.
