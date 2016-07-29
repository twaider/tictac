module.exports = [
  {
    "type": "heading",
    "defaultValue": "TicTac"
  },
  {
    "type": "text",
    "defaultValue": "Configuration"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },      
      {
        "type": "toggle",
        "messageKey": "BACKGROUND_ON",
        "label": "Enable Custom Hours Color",
        "defaultValue": false
      },
      {
        "type": "color",
        "messageKey": "BACKGROUND_COLOR",
        "defaultValue": "0x0055FF",
        "label": "Hour Hand Color"
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];