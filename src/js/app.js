// Import the Clay package
var Clay = require('pebble-clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig);
var messageKeys = require('message_keys');

// Listen for when the watchface is opened
Pebble.addEventListener(
    'ready', function(e) { console.log('PebbleKit JS ready!'); });

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage', function(e) {
  console.log('AppMessage received!');
  getWeather();
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) {
    return;
  }

  // Get the keys and values from each config item
  var dict = clay.getSettings(e.response);

  Pebble.sendAppMessage(
      dict, function(e) { console.log('Sent config data to Pebble'); },
      function(e) {
        console.log('Failed to send config data!');
        console.log(JSON.stringify(e));
      });
});