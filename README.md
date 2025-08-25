# Votive Candle Timer
Conversion of an on/off votive candle to a 2 hour single shot candle using an ATtiny25/45/85 and a tilt sensor. A magnet sensor (reed relay) is used to test many candles at the same time by illuminating for 5s.

# BOM
+ Candle: Homemory 100 Pack Flickering Flameless Votive Candles https://www.amazon.com/dp/B0CY54HJ3H
+ ATtiny25: I used a SOIC from Digikey
+ Tilt Switch: A mercury tilt switch would have been ideal, but they are banned in CA. This one uses a two ball bearings. Gikfun Metal Ball Tilt Shaking Position Switches Sw-520d (Pack of 20pcs) https://www.amazon.com/dp/B00RGN0KY0
+ Reed Relay: Waive a magnet above the candles to do a battery check. https://www.amazon.com/dp/B00RGN0KY0

Total COGS is about $3. Time per candle can be about 10 minutes with some planning (do each step to multiple candles before moving to next step).

# Implementation
The Homemory candle is simple. Just and LED (with flicker) and a battery. The "switch" simply moves one LED leg against the+ve side of the battery. The other LEG is bent under the -ve side of the batter and squished there with the battery cover.

I cut the LED lead between the "switch"/battery and soldered the leads to the SOIC - +ve battery side to pins 8 & 7, LED side to pins 6 & 5. It proved pretty solid. Then solder a wire from the other LED leg (-ve) to pin 4 and one terminal of the tilt sensor and reed relay. Then wire the reed relay and tisk sensor to the SOIC.
I UV eopoxied the tilt sensor and reed relay to the internal candle post (tilt sensor down facing).

No code was written by me (or any other human). Instead it was Claud Sonnet 4 coded using requiremnts only. I clued the AI into using the watchdog timer and deep sleep mode for both when the LED is on and off. The watchdog timer allows it to wake up once a second to check the elapsed time when the LED is on.
