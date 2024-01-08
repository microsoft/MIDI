// retrieves the current geolocation using the Windows.Devices.Geolocation API
var util = require('util');

var geolocation = require('windows.devices.geolocation');
var locator = new geolocation.Geolocator();

var msgTemplate = 'Your current location is: (%s, %s)';

locator.getGeopositionAsync( function(err, res) {
  // result is of type Windows.Devices.Geolocation.Geoposition
  if (err) {
    console.error(err);
    return;
  }

  console.info(util.format(msgTemplate, 
    res.coordinate.longitude, res.coordinate.latitude));
});