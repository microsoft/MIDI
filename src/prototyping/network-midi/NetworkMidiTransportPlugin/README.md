Shouldn't have needed to pull the ServiceContracts component directly into this project, but did it because of problems resolving types otherwise. PRototype. For the release code, it should be its own winmd that others can reference.


To build this, set up a local directory and unzip Boost into it. I'm currently using boost 1.81.0. Set your BOOST_ROOT environment variable to the root (d:\boost_1_81_0 in my case)

This also uses the latest Visual Studio 2022 pre-release.

C++/WinRT is installed through NuGet. Latest stable rev.


