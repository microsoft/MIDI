# Network MIDI 2 Proto

Covered by MIDI Association member agreements.

This driver / test app uses .NET 8 pre-release SDK. You'll need to install that separately.

Because of how project references work with WinRT, any time you make changes to the WinRT project, you'll need to remove and re-add the project reference in this project.



## Other Helpful commands

From command line

See what's being advertised 

dns-sd -B _midi2._udp
dns-sd -L "windowsproto" _midi2._udp

See active sockets

netstat -a -n -o -p UDP
