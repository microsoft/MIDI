---
layout: doc
title: MIDI Diagnostics Utility
---

# MIDI Diagnostics Utility

The Windows MIDI Services SDK installer comes with a simple command-line utility `mididiag.exe`. This has been designed for use by DAWs and support applications which need to shell out to a known executable to capture health information about the system.

The utility returns information only about MIDI. Here is example output broken down into sections.

```
TODO
```

We may add more fields or sections in the future. If you are machine parsing this file, do not rely on the order of fields or sections. The actual names of fields shown and the section headers will remain static and can be used in machine parsing, however. The tokens are not localized.

The date at the top of the file is in YYYY-MM-DD format. Time is in 24-hour format.

The executable returns 0 when succeeded, and non-zero when failed.
