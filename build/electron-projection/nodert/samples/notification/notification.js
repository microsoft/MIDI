// Show a windows notification from node.js using NodeRT modules
var util = require('util');

var xml = require('windows.data.xml.dom');
var notifications = require('windows.ui.notifications');

// for more info please see: http://msdn.microsoft.com/en-us/library/windows/apps/hh465448.aspx
var msgTemplate = '<toast><visual><binding template="ToastImageAndText02" branding="none">' + 
                        '<image id="1" src=""/>' + 
                         '<text id="1">%s</text>' +
                         '<text id="2">%s</text>' +
                  '</binding></visual></toast>';
                  
// use here an app id that was used for creating a shortcut
// for more info see: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802762
// and: https://github.com/nadavbar/node-win-shortcut
var appId = 'node_app_id';

// A Helper function for showing notifications 
function showNotification(title, text, onActiviation, onDismissal) {
  
  var toastXml = new xml.XmlDocument();
  toastXml.loadXml(util.format(msgTemplate, title, text));
  var toast = new notifications.ToastNotification(toastXml);
  
  toast.on("activated", function (sender, eventArgs) {
    onActiviation();
  });
  
  toast.on("dismissed", function () {
    onDismissal();
  });
  
  notifications.ToastNotificationManager.createToastNotifier(appId).show(toast);
}

showNotification('Hello NodeRT!', 'Please click here', function() {
  console.info('got event: toast activated!');
  process.exit(0);
}, function() {
  console.info('got event: toast dismissed!');
  process.exit(0);
});

// wait for user interaction:
setTimeout(function() {
  console.info('no event was fired');
}, 30000);