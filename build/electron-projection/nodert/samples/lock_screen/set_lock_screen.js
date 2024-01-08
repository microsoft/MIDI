var profile = require('windows.system.userprofile');
var storage = require('windows.storage');

var imageFileName = 'image.jpg';

storage.KnownFolders.picturesLibrary.getFileAsync(imageFileName, function(err, file) {
  if (err) {
    return console.error('Error getting image file:', err);
  }
  
  profile.LockScreen.setImageFileAsync(file, function(err) {
    if (err) {
        return console.error('Error setting lock screen image:', err);
    }
    console.info('Lock screen image was set successfully!');
  });
});