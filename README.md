# Votive Candle Timer
Conversion of an on/off votive candle to a 2-hour single-shot timed candle using an ATtiny25/45/85 and a tilt sensor to turn on (or off) by simply inverting.
A magnet sensor (reed relay) is used to do a bettery check on multiple candles simultaneously by illuminating each for 5s when a strong magnet is brought within a couple of inches - easy to waive over dozens of candles in a church environment (and an impressive demo).

# BOM
+ Candle: Homemory 100 Pack Flickering Flameless Votive Candles https://www.amazon.com/dp/B0CY54HJ3H
+ ATtiny25: I used a tube of 100 SOIC packaged devices from Digikey. Slightly cheaper than ATtiny85
+ Tilt Switch: A mercury tilt switch would have been ideal, but they are banned in CA. This one uses a two ball bearing contact sensor. Mounted pins upwards. Gikfun Metal Ball Tilt Shaking Position Switches Sw-520d (Pack of 20pcs) https://www.amazon.com/dp/B00RGN0KY0
+ Reed Relay: Wave a magnet above the candles to do a battery check. https://www.amazon.com/dp/B00RGN0KY0

Total COGS is about $3. Time per candle can be about 10 minutes with some planning (do each step to multiple candles before moving to next step).

Just FYI, this is the candle holder used in our church (apparently they "go missing" on occasion): https://www.concordiasupply.com/15-Hour-Votive-Light-Glasses-Ruby-Pack-of-12.

# Implementation
The Homemory candle is simple. Just an LED (with flicker) and a battery. The "switch" simply moves one LED lead against the +ve side of the battery. The other lead is bent under the -ve side of the battery and squished there with the battery cover.

I cut the LED lead between the "switch"/+ve battery and soldered the cut leads to the SOIC - +ve battery side to pins 8 & 7, LED side to pins 6 & 5. It proved pretty solid by doing it this way. Then solder a wire from the other LED leg (-ve) to pin 4 and to one terminal of the tilt sensor and reed relay. Then wire the other end of the reed relay and tilt sensor to the SOIC pins 2 and 3.

I UV epoxied the tilt sensor and reed relay to the internal candle post (tilt sensor down facing).

I placed a label with a QR code of this Github repository URL on the inside of the candle.

No code was written by me (or any other human). Instead it was Claud Sonnet 4 generated using requirements only. I clued the AI into using the watchdog timer and deep sleep mode for both when the LED is on and off. The watchdog timer allows it to wake up once a second to check the elapsed time when the LED is on.

The ATtiny25 uses only 7uA in all states (apart from the infinitesimal time it wakes to check its timer). The LED takes about 1mA. It works down to below 3V, by which time the LED gets pretty dim (looks the same brightness in this candle or an unmodified one). I figure it will last about a month in a church environment.

Tip - use AVRDUDE to program multiple ATtiny25 devices rather than Arduino (which needs to rebuild each time).
