var Clay = require('pebble-clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

function parseDaysBitmask(val) {
  if (typeof val === 'number') {
    return val;
  }
  if (!Array.isArray(val)) {
    return 127; // default all days
  }
  // Options: ["Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"]
  // Index 0: Lun (tm_wday 1) -> bit 1
  // Index 1: Mar (tm_wday 2) -> bit 2
  // Index 2: Mer (tm_wday 3) -> bit 3
  // Index 3: Gio (tm_wday 4) -> bit 4
  // Index 4: Ven (tm_wday 5) -> bit 5
  // Index 5: Sab (tm_wday 6) -> bit 6
  // Index 6: Dom (tm_wday 0) -> bit 0
  var dayToBit = [1, 2, 3, 4, 5, 6, 0];
  var mask = 0;
  for (var i = 0; i < val.length; i++) {
    if (val[i]) {
      mask |= (1 << dayToBit[i]);
    }
  }
  return mask;
}

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) {
    return;
  }

  var rawSettings = clay.getSettings(e.response, false);
  var dict = {};

  for (var key in rawSettings) {
    if (!rawSettings.hasOwnProperty(key)) continue;
    var item = rawSettings[key];
    var val = item ? item.value : undefined;
    if (val === undefined) continue;

    if (key === 'Alarm1Days' || key === 'Alarm2Days' || key === 'Alarm3Days') {
      if (messageKeys[key] !== undefined) {
        dict[messageKeys[key]] = parseDaysBitmask(val);
      }
    } else if (typeof val === 'boolean') {
      if (messageKeys[key] !== undefined) {
        dict[messageKeys[key]] = val ? 1 : 0;
      }
    } else if (messageKeys[key] !== undefined) {
      dict[messageKeys[key]] = val;
    }
  }

  console.log('Invio impostazioni a Pebble: ' + JSON.stringify(dict));
  Pebble.sendAppMessage(dict, function() {
    console.log('Impostazioni inviate con successo a Pebble');
  }, function(err) {
    console.log('Errore invio impostazioni: ' + JSON.stringify(err));
  });
});
