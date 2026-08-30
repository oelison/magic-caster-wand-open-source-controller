# Magic Caster Wand – Open Source Controller

An unofficial open-source ESP-based controller for the original **Harry Potter: Magic Caster Wand**.

This project allows the original Magic Caster Wand to be used **without the official smartphone app**. An ESP-based controller detects the wand, receives its gesture and motion data, and processes the detected spells directly.

The wand itself is **not modified**. No firmware is installed on the wand and no hardware changes are required.

The ESP controller effectively replaces the smartphone/app as the device that receives and processes the wand's data. This makes it possible to use the wand as a standalone controller without requiring a smartphone to be present or running.

### Features

* Works with the original Magic Caster Wand hardware
* No modification of the wand required
* No smartphone required during operation
* No smartphone app
* ESP-based standalone operation
* Receives and processes wand motion/gesture data
* Allows custom actions to be assigned to spells and gestures
* Open-source implementation

## Restrictions and Known Limitations

The current implementation has the following limitations:

1. **Spell gestures**
   Currently, only spells consisting of straight line segments and corners are supported. Gestures containing circles or curved sections are not yet supported.

2. **MQTT / Local Broadcast**
   MQTT or a local broadcast mechanism is not implemented yet. This is planned for future versions to allow detected spells and wand events to be distributed to other systems, such as home automation.

3. **Patronus and House**
   Patronus and Hogwarts House cannot currently be configured. These settings are available in the original smartphone app but are not currently used by the ESP controller.

4. **Compatibility with the original app's broadcast system**
   The original smartphone app transmitted wand events via local broadcasts. My previous ioBroker integration (not added to ioBroker) used these broadcasts to receive wand events and integrate them into my home automation system.

   The smartphone app is no longer usable, so these broadcasts are no longer available. Reimplementing the broadcast protocol would be possible, but would require implementing the corresponding **House and Patronus configuration**, as these values were used to distinguish between different wands or installations (for example, Wand 1 and Wand 2).

   The current ESP-based approach avoids this dependency by communicating with the wand directly.

5. **Callibration**
   Simply missing. (ToDo)


### Disclaimer

This is an unofficial, independent open-source project and is not affiliated with, sponsored by, or endorsed by Warner Bros. Entertainment, Wizarding World, J.K. Rowling, or their respective licensors.

**Harry Potter**, **Magic Caster Wand**, **Lumos**, **Protego**, and other related names and characters are the property of their respective rights holders.

The project uses the original Magic Caster Wand hardware without modifying the wand itself and is intended to provide an independent controller for that hardware.

### License

MIT License

Copyright (c) 2026 Christian Oelschlegel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


