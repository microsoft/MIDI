# Redistribution

> NOTE: This is a stub and will be fleshed out when we are closer to shipping.

Here's what you can redistribute with your own apps

> "App" in the context of this file refers to a **Windows application which adds significant functionality above and beyond what Windows MIDI Services provides.** This is an open source project, but we prefer developers not create their own Windows MIDI Services redistribution packages or forks for Windows, but instead contribute back to this main project. We don't want to fragment the ecosystem with multiple incompatible or functionally different versions. Nothing legal will stop you from fragmenting, but it's not good for musicians and other customers.
> Of course, versions for other operating systems are not covered by above. This project has a permissive MIT license which allows (and encourages) ports to other operating systems. Those will have their own installation approach and requirements.

## Required Components

Your app may ship any of the required referenced SDK components. We will provide an MSI and/or merge module to enable that.

## Service, Driver, and Tools

Although we prefer apps to link to the latest installer, some apps want to have everything available in a single download. In those cases, the installer(s) may be redistributed. If your app normally downloads other components from the web, however, we recommend you always download the latest compatible installer for Windows MIDI Services

TODO: We will look at creating a MSI merge module or self-extracting package you can integrate into your own setup.

TODO: Need to make final recommendation on driver, which will be available through other means.
