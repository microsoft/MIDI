# Source

The main source tree.

| Location | Description |
| -------------------- | ----------------------------------------------------- |
| installers |  End-user and developer installers for service, sdk, tools, etc.  |
| plugins | Transport, processing, and other plugins |
| sdk | App developer SDKs for MIDI |
| service | The Windows Service   |
| service-protocol-shared | Internal communications libraries between SDK and service |
| shared | Libraries that aren't part of the SDK, but are internally shared in the projects |
| tools | End-user tools like settings and the CLI   |
| usb-driver | USB MIDI 2.0 driver |

The Main MIDI Solution.sln file includes the service, shared libraries, SDK, etc. It does not
include the tools, driver, and other parts. When working on a project, we recommend
creating a solution which includes only the projects you plan to change, and uses compiled
versions of other components. 
