---
layout: sdk_namespace_page
title: WinRT API Client Message processing Plugins Overview
namespace: Windows.Devices.Midi2.ClientPlugins
description: Namespace with built-in client-side message processing plugins
---

This namespace has the message processing plugins that come with the API. They run in your application's process. Each one watches a connection and passes on only the messages you ask for, so one connection can serve several parts of your application. Attach them with `MidiEndpointConnection.AddMessageProcessingPlugin` before you open the connection.