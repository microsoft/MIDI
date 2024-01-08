NodeRT sample - showing a notification on the desktop
=====================================================

A sample script for showing a notification on the desktop.

**Perquisites**:

In order to show notifications from a desktop application you need to have a shortcut with an **app-id**
More info on this is available here: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802762

You can create a shortcut in node.js easily using the following node module: https://www.npmjs.org/package/node-win-shortcut:

First, Run: 
```javascript
npm install node-win-shortcut
```

Then, you can run the following node.js code in order to create the shortcut with the app-id (you can use whatever string you like instead of 'node_app_id'):

```javascript
var win_shortcut = require('node-win-shortcut');
win_shortcut.createShortcut(process.execPath, 'node', 'node_app_id');
```

After you've created the shortcut, replace 'node_app_id' in the following line in the notification.js script with your shortcut's app-id (if necessary):

```javascript
var appId = 'node_app_id';
```

In order to run this sample you should generate the following namespaces and place them in a node_modules directory near the script file:

* Windows.Data.Xml.Dom
* Windows.Ui.Notifications
