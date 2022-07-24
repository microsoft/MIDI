# API Specifications

=======================================================
TODO: This needs to be updated after the SDK changes
=======================================================

These are not final designs, but rather are brainstorming design ideas. All code is pseudo-c code with just enough syntax to get the point across. You will notice a real lack of semicolons, for example. :)

## Setups

Setups are saved JSON configurations created by the user in the settings app. They contain custom
device and endpoint names, virtual ports, routing, plugin settings and more. Creating or changing
them via code was considered, but rejected due to end-user control and potential security concerns.

We will revisit if new requirements arise.

## Config files location

(HKLM reg key)

## File format

