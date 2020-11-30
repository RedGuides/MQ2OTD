# MQ2OTD

Adds an icon to the HUD indicating where your target is located in relation to you and how far away it is.

# Prerequisites

Requires your HUD to be on (F11)

# Information and Usage

## Commands

`/otd on` Toggle indicator on.  
`/otd off` Toggle indicator off.  
`/otd moveto x y` Move the indicator to the x,y screen coordinates given and save them to the plugins .ini.  
`/otd moveby x y` Move the indicator by x,y increment from the current x,y location on the screen.  
`/otd range` Toggles target range indicator on or off, it is defaulted to on.  
`/otd reload` Reload the plugin's .ini.  
`/otd drawtight` Will draw the range indicator very close the direction indicator in the HUD.  
`/otd getloc` Display the current x,y screen coordinates of the direction indicator.  
`/otd dump` Display the current color settings.  
`/otd version` Display the plugin version.  

## Ini

The [COLORS] section in the ini has the following format:  
RANGECOLORN=HEX_RGB_COLOR RANGE COLOR_MORPH_FLAG  
LABELCOLOR=HEX_RGB_COLOR

Note: these setting must be in ascending range order, or none will be loaded (defaults used instead).

Example:

```ini
[COLORS]
LABELCOLOR=19FF19
RANGECOLOR1=FF1919 10 0
RANGECOLOR2=1919FF 11 0
RANGECOLOR3=19FF19 200 1
RANGECOLOR4=C8C820 201 0
RANGECOLOR5=505020 500 1
RANGECOLOR6=F0F0F0 501 0
```

## Authors

* **OmniCtrl**
