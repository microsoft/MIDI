---
layout: sdk_reference_page
title: MidiRuntimeVersion
namespace: Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Version information for the installed MIDI runtime.
implements: Windows.Foundation.IStringable

---

Represents a mostly-SemVer-compatible view of a MIDI App SDK Runtime version.

## Members

| Function | Description |
| --------------- | ----------- |
| `Major` | Major Version. Changes when compatibility breaks.  |
| `Minor` | Minor Version. Changes for new features, but maintains compatibility. |
| `Patch` | Bug fixes. Maintains compatibility. |
| `BuildNumber` | Internal build number. This resets for each patch. |
| `PreviewSuffix` | If a preview, this will contain text like `preview.11.2507` |

## Functions

| Function | Description |
| --------------- | ----------- |
| `IsGreaterThan` | Returns true if this version is higher than the provided argument. This does take BuildNumber into account, which strict SemVer does not. |

[More about Semantic Versioning 2.0.0](https://semver.org/)
