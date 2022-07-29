# USB MIDI 2.0 Class and Device Driver

See driver requirements spec in /specs for details.

## Potentially Helpful References

Here are some potentially helpful resources for driver development for Windows 10 v2004 and above. Not all of this information necessarily applies to this driver, but is here to help the driver team make implementation decisions.

Driver Dev Portal

* [Portal](https://docs.microsoft.com/windows-hardware/drivers/)

Driver Frameworks

* [KMDF Version history (v1.31 is what we'll need for Windows 10)](https://docs.microsoft.com/windows-hardware/drivers/wdf/kmdf-version-history)
* [What's new for WDF in Windows 10](https://docs.microsoft.com/windows-hardware/drivers/wdf/)
* [UMDF vs KMDF](https://docs.microsoft.com/windows-hardware/drivers/wdf/comparing-umdf-2-0-functionality-to-kmdf)
* [Sample KMDF Drivers](https://docs.microsoft.com/windows-hardware/drivers/wdf/sample-kmdf-drivers)

USB Drivers

* [Choosing a driver model for USB devices](https://docs.microsoft.com/windows-hardware/drivers/usbcon/winusb-considerations)
* [USB Driver Development](https://docs.microsoft.com/windows-hardware/drivers/usbcon/getting-started-with-usb-client-driver-development)
* [Write a USB client driver (KMDF)](https://docs.microsoft.com/windows-hardware/drivers/usbcon/tutorial--write-your-first-usb-client-driver--kmdf-)
* [USB Driver Samples](https://docs.microsoft.com/windows-hardware/drivers/usbcon/usb-driver-samples-in-wdk)

Memory and Buffer Sharing

* [Memory management for drivers](https://docs.microsoft.com/windows-hardware/drivers/kernel/managing-memory-for-drivers)
* [Methods for accessing data buffers](https://docs.microsoft.com/windows-hardware/drivers/kernel/methods-for-accessing-data-buffers)
    * [Using Buffered I/O](https://docs.microsoft.com/windows-hardware/drivers/kernel/using-buffered-i-o)
    * [Using DIrect I/O](https://docs.microsoft.com/windows-hardware/drivers/kernel/using-direct-i-o)

Installation

* [How devices and drivers are installed in Windows](https://docs.microsoft.com/windows-hardware/drivers/install/)
* [Writing INF Files](https://docs.microsoft.com/ewindows-hardware/drivers/install/writing-inf-files)
* [Installing private builds of inbox drivers](https://docs.microsoft.com/windows-hardware/drivers/install/overview-of-installing-private-builds-of-in-box-drivers)

Tools

* [Verifying drivers](https://docs.microsoft.com/en-us/windows-hardware/drivers/devtest/tools-for-verifying-drivers)

Outdated info

* [Class Driver and Minidriver](https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/class-driver-and-minidriver-definitions)
* [Streaming Minidrivers](https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/streaming-minidrivers2)
