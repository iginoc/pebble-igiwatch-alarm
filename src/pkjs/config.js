module.exports = [
  {
    "type": "heading",
    "defaultValue": "IgiWatchAlarm Configuration"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },
      {
        "type": "color",
        "messageKey": "BackgroundColor",
        "defaultValue": "0x000000",
        "label": "Background"
      },
      {
        "type": "color",
        "messageKey": "TextColor",
        "defaultValue": "0xFFFFFF",
        "label": "Text"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Font"
      },
      {
        "type": "radiogroup",
        "messageKey": "FontChoice",
        "label": "Choose Font",
        "defaultValue": "0",
        "options": [
          { "label": "Avenir Next", "value": "0" },
          { "label": "Leco 1976", "value": "1" },
          { "label": "Avenir Next Regular", "value": "2" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Orientation"
      },
      {
        "type": "radiogroup",
        "messageKey": "ScreenOrientation",
        "label": "Choose Orientation",
        "defaultValue": "1",
        "options": [
          { "label": "Always Portrait", "value": "1" },
          { "label": "Always Landscape (Left)", "value": "2" },
          { "label": "Always Landscape (Right)", "value": "3" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Date Language"
      },
      {
        "type": "radiogroup",
        "messageKey": "DateLanguage",
        "label": "Choose Language (Portrait Mode Only)",
        "defaultValue": "it_IT",
        "options": [
          { "label": "Italian", "value": "it_IT" },
          { "label": "English", "value": "en_US" },
          { "label": "French", "value": "fr_FR" },
          { "label": "German", "value": "de_DE" },
          { "label": "Spanish", "value": "es_ES" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Sveglia 1"
      },
      {
        "type": "toggle",
        "messageKey": "Alarm1Active",
        "label": "Abilita Sveglia 1",
        "defaultValue": false
      },
      {
        "type": "input",
        "messageKey": "Alarm1Time",
        "defaultValue": "07:00",
        "label": "Orario Sveglia 1",
        "attributes": {
          "type": "time"
        }
      },
      {
        "type": "checkboxgroup",
        "messageKey": "Alarm1Days",
        "label": "Giorni attivi",
        "defaultValue": [true, true, true, true, true, false, false],
        "options": ["Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Sveglia 2"
      },
      {
        "type": "toggle",
        "messageKey": "Alarm2Active",
        "label": "Abilita Sveglia 2",
        "defaultValue": false
      },
      {
        "type": "input",
        "messageKey": "Alarm2Time",
        "defaultValue": "08:00",
        "label": "Orario Sveglia 2",
        "attributes": {
          "type": "time"
        }
      },
      {
        "type": "checkboxgroup",
        "messageKey": "Alarm2Days",
        "label": "Giorni attivi",
        "defaultValue": [true, true, true, true, true, true, true],
        "options": ["Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Sveglia 3"
      },
      {
        "type": "toggle",
        "messageKey": "Alarm3Active",
        "label": "Abilita Sveglia 3",
        "defaultValue": false
      },
      {
        "type": "input",
        "messageKey": "Alarm3Time",
        "defaultValue": "09:00",
        "label": "Orario Sveglia 3",
        "attributes": {
          "type": "time"
        }
      },
      {
        "type": "checkboxgroup",
        "messageKey": "Alarm3Days",
        "label": "Giorni attivi",
        "defaultValue": [false, false, false, false, false, true, true],
        "options": ["Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"]
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Salva Impostazioni"
  }
];
