# Shared

Shared source code used by API, tools, etc. but which is not application-facing API surface area.

Examples include configuration reading/writing, error logging, and more.

Also includes shared assets like icons. Some considerations there:
To keep this open to end-user additions at runtime, we'll want to split these into light and dark mode versions at a minimum. We likely don't need to go the route that UWP does with high DPI and asset names, but that's a possible consideration.

We've also considered just rolling things into a font (which provides other benefits besides light/dark, such as being able to have solid underlays which provide color) but this is not as friendly to end-user customization. Some folks are likely going to want to use actual photos of their gear.