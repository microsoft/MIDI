# Developers

## Want to build apps using the MIDI API and SDK?

See [app-developers section](app-developers/)

## Want to contribute to the project?

See [contributors section](contributors/)

## Yet another API?

Wait, is this just yet another "API to rule them all", so we end up with n+1 APIs to deal with?

Yes and no. Part of the reason this is open source is to make everything as visible as possible, and to allow much-needed input from app developers. The long-term intention is to discourage use of the older APIs through eventual deprecation, and to encourage use of this new API by adding in all the features and capabilities that users and app authors want. We simply couldn't do that with any of the older APIs, which were tied heavily to an older driver model, and so we've ended up with a new one that we can grow to support the features and the evolution of the standards.

## What else does this provide for developers?

MIDI 2.0 is a standard that allows for many different types of physical transport layers. Developers often want to experiment with new or novel approaches for getting MIDI information from one device to another. In the past, that has required either creating a MIDI stack from scratch (which was not difficult with the simpler MIDI 1.0), or writing a Windows driver.

We want to encourage experimentation. By adopting a pluggable transport model for the Windows MIDI Services, it's possible for a developer to much more quickly prototype an idea, or an evolving standard, and see how it performs in action, all without climbing the driver development hill.

Developer FAQs:

* [Programming Languages, WinRT, and Projections](FAQ-Programming-Languages.md)