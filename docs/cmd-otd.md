---
tags:
  - command
---

# /otd

## Syntax

<!--cmd-syntax-start-->
```eqcommand
/otd [on|off|range|reload|drawtight|getloc] | [moveto <x y>] | [moveby <x y>]
```
<!--cmd-syntax-end-->

## Description

<!--cmd-desc-start-->
Toggles the compass on and off, moves it around the screen, and configures some settings.
<!--cmd-desc-end-->

## Options

| Option | Description |
|--------|-------------|
| `(no option)` | displays help text |
| `version` | displays the plugin version |
| `on|off` | Toggle the compass display |
| `moveto <x y>` | Move the indicator to the x,y screen coordinates given and save them to MQ2OTD.ini |
| `moveby <x y>` | Move the indicator incrementally around the screen |
| `range` | Toggles target range indicator on or off, it is defaulted to on. |
| `reload` | reloads the INI |
| `drawtight` | Will draw the range indicator very close the direction indicator in the HUD |
| `getloc` | Display the current x,y of the compass on your screen |
| `colorchange|dump` | undocumented commands that are no longer used |

## Examples

- move the compass to 0,0
: `/otd moveto 0 0`
- Move the compass 5 pixels to the right
: `/otd moveby 5 0`