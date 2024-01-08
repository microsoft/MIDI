var nodert_streams = require('nodert-streams');
var speech = require('windows.media.speechsynthesis');
var fs = require('fs');
var path = require('path');

// taken from this great post: http://tomasz.janczuk.org/2014/06/playing-audio-from-nodejs-using-edgejs.html
var edge = require('edge');   
var play = edge.func(function() {/*
     async (input) => {
         return await Task.Run<object>(async () => {
             var player = new System.Media.SoundPlayer((string)input);
             player.PlaySync();
             return null;
         });
    }
*/});

var outputFilePath = path.join(__dirname, 'synthesized_text.wav');

var synth = new speech.SpeechSynthesizer();
synth.synthesizeTextToStreamAsync('Hello Node R T!', function(err, res) {
    if (err) {
        return console.info(err);
    }
    var inputStream = new nodert_streams.InputStream(res);
    var fileStream = fs.createWriteStream(outputFilePath);
    fileStream.on('close', function () {
        play(outputFilePath, function (err) {
            if (err) return console.error(err);
            fs.unlinkSync(outputFilePath);
       });
    });

    inputStream.pipe(fileStream);
});