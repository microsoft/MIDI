---
layout: kb
title: Restoring endpoint settings after a device identifier changes
audience: everyone
description: Why the name, picture and other settings you saved for a MIDI device can stop being applied, what Windows does about it, and how to put them back.
---

# Restoring endpoint settings after a device identifier changes

You gave a MIDI device a name, chose a picture for it, perhaps renamed its MIDI 1.0 ports. Later you plugged it into a different USB port, or into a hub, and everything you set is gone. The device works, but it is back to the name it came with.

Nothing was lost. Your settings are still saved. They are just no longer being matched to that device, and this article explains why and how to put them back.

## Why it happens

Windows MIDI Services stores your settings against the endpoint's identifier, and it deliberately matches on that identifier and nothing else. Matching on softer things, such as the manufacturer name or the USB vendor and product numbers, was tried and removed: several devices can report identical values, and some report the same placeholder serial number as each other, so it applied the wrong customer's settings to the wrong device.

For a USB device, that identifier is derived from how Windows identifies the device, and that in turn depends on whether the device reports a serial number.

- **A device that reports a real serial number** keeps the same identity wherever you plug it in. Its settings follow it from port to port.
- **A device that reports no serial number** is identified by the hub and port it is plugged into, because that is the only thing distinguishing it from another one of the same model. Move it and Windows considers it a different device.

Most USB devices do not report a serial number. This is a property of the hardware, not something Windows or the device driver can change.

The same thing happens if you move your configuration file to a new computer, and it can happen after a Windows update if device migration does not complete.

## What Windows does about it

The saved entry is kept, not deleted. If you plug the device back into the port it was in before, the settings are applied again immediately, with no action from you.

If you do not, the entry sits there matching nothing. Windows MIDI Services calls that an **orphaned** customization. It is a prompt to re-link, not a fault.

## Putting the settings back

### Using MIDI Settings

Open **MIDI Settings**. If any saved settings match nothing connected, a message appears above the endpoint list. Choose **Review them**.

The left side lists the saved settings which match nothing, shown with the picture you chose, the name you gave them, and a summary of what each one holds. The right side lists the devices those settings could be moved onto.

Windows puts the most likely device at the top and says why, for example that it is the same model and the only one connected. **It does not decide for you.** When nothing is clearly the best match, nothing is selected and the button stays disabled, because guessing would put your settings on the wrong instrument.

Choose the settings you recognize, choose the device, and select **Move it here**. If you no longer want an entry, **Delete it** removes it.

The message can be dismissed. Once dismissed, you will not be asked about those same entries again.

### Using the MIDI console

```
midi endpoint customizations list
midi endpoint customizations relink --from <stored id> --to <endpoint device id>
midi endpoint customizations forget --from <stored id>
```

`list` shows every saved customization, whether each one currently matches a device, and what it holds. `relink` moves one onto a different endpoint. `forget` deletes one. Add `--orphaned` to `list` to see only the entries with no matching device.

## Entries which hold nothing

Some saved entries contain nothing you would miss, usually because an older version of the settings app wrote an entry when you opened a dialog and pressed Save without changing anything. These are hidden from the review list, and counted separately at the bottom of `midi endpoint customizations list`. They do no harm.

## What is worth most

If you have measured an output latency value for a device, that is the hardest setting to reproduce: it needed a loopback cable and a measurement. Windows records whether a stored latency was measured or typed in, and shows it, so you can tell before you delete something whether you can easily get it back.

## Avoiding it in future

If your device reports a serial number, its settings will follow it between ports and this will not happen. If it does not, the only reliable way to avoid re-linking is to keep using the same USB port, or the same port on the same hub.
