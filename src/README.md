# Source

The main source tree.

| Location | Description |
| -------------------- | ----------------------------------------------------- |
| diagnostics | Basic performance diagnostics for the PC  |
| installers | End-user and developer installers for service, sdk, tools, etc.  |
| plugins | Transport, processing, and other plugins |
| sdk-client | App developer API/SDKs for MIDI. Currently C#, but will be C++ in final version |
| sdk-plugin | Service plugin developer API/SDKs |
| service | The Windows Service |
| service-protocol-shared | Internal communications libraries between SDK and service |
| shared | Libraries that aren't part of the SDK, but are internally shared in the projects |
| tools | End-user tools like settings and the CLI |
| usb-driver | USB MIDI 2.0 driver |

The Main MIDI Solution.sln file includes the service, shared libraries, SDK, etc. It does not
include the tools, driver, and other parts. When working on a project, we recommend
creating a solution which includes only the projects you plan to change, and uses compiled
versions of other components.

