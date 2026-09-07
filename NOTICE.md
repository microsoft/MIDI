THIRD PARTY SOFTWARE NOTICES AND INFORMATION
Do Not Translate or Localize

Windows MIDI Services incorporates material from the projects listed below. The original
copyright notices and the licenses under which Microsoft received such material are set forth
below, for informational purposes only. Microsoft reserves all rights not expressly granted
herein, whether by implication, estoppel or otherwise.

This software incorporates material from third parties. Microsoft makes certain open source
code available at https://3rdpartysource.microsoft.com, or you may send a check or money order
for US $5.00, including the product name, the open source component name, platform, and version
number, to:

    Source Code Compliance Team
    Microsoft Corporation
    One Microsoft Way
    Redmond, WA 98052
    USA

Notwithstanding any other terms, you may reverse engineer this software to the extent required
to debug changes to any libraries licensed under the GNU Lesser General Public License.

Windows MIDI Services itself is licensed under the MIT License. See LICENSE in the root of this
repository.

---------------------------------------------------------

## Index

### Components linked into shipping binaries

| # | Component | License | Where it is used |
|---|---|---|---|
| 1 | AM_MIDI2.0Lib (`libmidi2`) | MIT | Windows MIDI Services SDK, the bytestream/UMP transforms, the UMP protocol downscaler, the KS Aggregate and Bluetooth transports |
| 2 | {fmt} | MIT | `midi.exe` console, `midi1enum`, `midi1monitor`, `mididiag`, `midiksinfo` |
| 3 | CLI11 | BSD 3-Clause | `midi.exe` console command-line parsing |
| 4 | FTXUI | MIT | `midi.exe` console interactive pickers, prompts and formatted output |
| 5 | Boost (`circular_buffer` and its dependencies) | Boost Software License 1.0 | Network MIDI 2.0 transport, Bluetooth MIDI transport |
| 6 | USB MIDI 2.0 class driver source (AMEI / AmeNote) | MIT | `usbmidi2.sys` USB MIDI 2.0 class driver |
| 7 | TinyUSB `tusb_ump` | MIT | `ump.h` in the USB MIDI 2.0 class driver |

### Build-time and development-time dependencies

These are not linked into shipping binaries, but are required to build this repository.

| Component | License | Notes |
|---|---|---|
| vcpkg | MIT (Microsoft) | Vendored in this repository under `vcpkg/`. See `vcpkg/NOTICE.txt` and `vcpkg/LICENSE.txt` for its own third-party notices. |
| Windows Implementation Library (WIL) | MIT (Microsoft) | NuGet `Microsoft.Windows.ImplementationLibrary`. Carries its own `ThirdPartyNotices.txt`. |
| C++/WinRT | MIT (Microsoft) | NuGet `Microsoft.Windows.CppWinRT` |
| C#/WinRT | MIT (Microsoft) | NuGet `Microsoft.Windows.CsWinRT` |
| Windows App SDK | Microsoft Software License | NuGet `Microsoft.WindowsAppSDK` |
| Windows SDK Build Tools | Microsoft Software License | NuGet `Microsoft.Windows.SDK.BuildTools` |
| .NET Community Toolkit | MIT (.NET Foundation and Contributors) | NuGet `CommunityToolkit.WinUI.Collections` |
| WiX Toolset | Microsoft Reciprocal License (MS-RL) | Used only to build the installers |

### Samples (`samples/`)

Not part of any shipping binary. See section 8 below.

### Scope

This file does not cover the Jekyll documentation site in `docs/`, which is being retired in
favor of Microsoft Learn.

---------------------------------------------------------

## 1. AM_MIDI2.0Lib (`libmidi2`)

https://github.com/midi2-dev/AM_MIDI2.0Lib

Consumed through vcpkg. The port manifest used by this repository is in
`src/in-box/Libs/LibMidi2/overlay-ports/`.

%% AM_MIDI2.0Lib NOTICES AND INFORMATION BEGIN HERE
=========================================
MIT License

Copyright (c) 2021 Andrew Mee

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
=========================================
END OF AM_MIDI2.0Lib NOTICES AND INFORMATION

---------------------------------------------------------

## 2. {fmt}

https://github.com/fmtlib/fmt

%% {fmt} NOTICES AND INFORMATION BEGIN HERE
=========================================
Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
=========================================
END OF {fmt} NOTICES AND INFORMATION

---------------------------------------------------------

## 3. CLI11

https://github.com/CLIUtils/CLI11

%% CLI11 NOTICES AND INFORMATION BEGIN HERE
=========================================
CLI11 2.6.2 Copyright (c) 2017-2026 University of Cincinnati, developed by Henry
Schreiner under NSF AWARD 1414736. All rights reserved.

Redistribution and use in source and binary forms of CLI11, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=========================================
END OF CLI11 NOTICES AND INFORMATION

---------------------------------------------------------

## 4. FTXUI

https://github.com/ArthurSonzogni/FTXUI

%% FTXUI NOTICES AND INFORMATION BEGIN HERE
=========================================
The MIT License

Copyright (c) 2019 Arthur Sonzogni.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
=========================================
END OF FTXUI NOTICES AND INFORMATION

---------------------------------------------------------

## 5. Boost

https://www.boost.org/

Used through the vcpkg `boost-circular-buffer` port and the Boost packages it depends on.

%% Boost NOTICES AND INFORMATION BEGIN HERE
=========================================
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
=========================================
END OF Boost NOTICES AND INFORMATION

---------------------------------------------------------

## 6. USB MIDI 2.0 class driver source (AMEI / AmeNote)

The source for the USB MIDI 2.0 class driver in `src/in-box/Drivers/USBMIDI2/` was developed by
AmeNote for the Association of Musical Electronics Industry (AMEI), and is jointly copyrighted
with Microsoft. The same notice appears at the top of each source file in that directory.

%% USB MIDI 2.0 class driver NOTICES AND INFORMATION BEGIN HERE
=========================================
Copyright 2023 Association of Musical Electronics Industry
Copyright 2023 Microsoft
Driver source code developed by AmeNote. Some components Copyright 2023 AmeNote Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
=========================================
END OF USB MIDI 2.0 class driver NOTICES AND INFORMATION

---------------------------------------------------------

## 7. TinyUSB `tusb_ump`

https://github.com/hathach/tinyusb

`src/in-box/Drivers/USBMIDI2/Driver/ump.h` is adapted from the `tusb_ump` library.

%% TinyUSB NOTICES AND INFORMATION BEGIN HERE
=========================================
The MIT License (MIT)

Copyright (c) 2019 Ha Thach (tinyusb.org)
Copyright (c) 2022 Michael Loh (AmeNote.com)
Copyright (c) 2022 Franz Detro (native-instruments.de)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
=========================================
END OF TinyUSB NOTICES AND INFORMATION

---------------------------------------------------------

## 8. Samples

The samples in `samples/` are illustrative and are not part of any shipping binary. Beyond the
Microsoft-published packages listed above, they reference:

| Component | Version | Copyright | License | Used by |
|---|---|---|---|---|
| WinUIEx | 2.8.0 | Morten Nielsen | MIT | `samples/csharp-net/virtual-device-app-winui` |
| Electron | 39.x | OpenJS Foundation and Electron contributors | MIT | `samples/electron-js/electron-api-basics` (development dependency) |
