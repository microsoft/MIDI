NodeRT sample - using speech synthesis and streams
=========================================

A sample script for synthesizing text to speech using the Windows.Media.SpeechSynthesis API, writing the output audio stream to file using nodert_streams, and then playing it using the media player.

In order to play the audio file, this sample uses <a href="https://www.npmjs.org/package/edge" target="_blank">edge.js</a> and a code snippet from this <a href="http://tomasz.janczuk.org/2014/06/playing-audio-from-nodejs-using-edgejs.html" target="_blank">great blog post</a>.

**Perquisites**:

Install edge.js by running:
```
npm install edge
```

This sample uses the <a href="https://github.com/NodeRT/nodert-streams">nodert-streams</a> module. 
In order to use it - clone it to your machine to a node_modules directory near the script, then build the native bindings of nodert-streams by running the following command from inside the nodert-streams directory:

```
node-gyp rebuild
```

In order to run this sample you should generate the following namespaces and place them in a node_modules directory near the script file:

* Windows.Media.SpeechSynthesis
* Windows.Storage.Streams (for nodert-streams)
