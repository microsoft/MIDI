/************************************************************************************
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
************************************************************************************/
/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

#include <initguid.h>

DEFINE_GUID (GUID_DEVINTERFACE_USBUMPDriver,
    0x5d62a9ab,0xaa0d,0x4144,0xb0,0x83,0x1f,0x97,0x18,0xfa,0x94,0xb6);
// {5d62a9ab-aa0d-4144-b083-1f9718fa94b6}

#define IOCTL_INDEX             0x0000

#define IOCTL_USBUMPDRIVER_GET_CONFIG_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#define IOCTL_USBUMPDRIVER_GET_MFGNAME         CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX+1,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#define IOCTL_USBUMPDRIVER_GET_DEVICENAME      CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX+2,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#define IOCTL_USBUMPDRIVER_GET_SERIALNUM       CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX+3,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#define IOCTL_USBUMPDRIVER_GET_GTBDUMP         CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX+4,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#define IOCTL_USBUMPDRIVER_GET_DEVICETYPE     CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX+5,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)
