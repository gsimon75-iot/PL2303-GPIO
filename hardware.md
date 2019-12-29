# Hardware considerations

On the usual commercial PL2303 controllers it's rare that all pins are lead out nicely to at least test pads, usually
it's just RXD and TXD. Sometimes the handshake signals are hardwired to loopback: RTS with CTS, DTR with DSR.

In such cases the only way is to de-solder the chip from the original pcb and re-solder it to a TSSOP28 breakout board.
The drawback is that it's needs some really fine soldering, so don't even start it if your hands aren't absolutely steady.

For de-soldering the chip without melting its internals and breaking its pins, I recommend the following procedure, though
don't blame me if it doesn't work for you.

First of all, trace the schematic, what resistors and what capacitors are connected between what pins, etc.


## De-soldering the chip

- Separate a blade from a disposable razor. It's thinner than normal Gilette blades and its surface repels the soldering tin.
- Be *extremely* careful with that blade, it's mighty sharp and it has no handle: grab it *only* with a plier or a tweezer!

- Choose the clumsiest tip you got with your soldering iron, the one that you'll never use anyway
- Fasten it into a power screwdriver, spin it up, and use a file to re-shape it as pointy as you can
- Get a honing stone and continue, you can get it even sharper :)

- Fasten the original pcb into a jewelers vice or that '3rd hand' soldering stand (with adjustable clips and magnifier)
- Attach your super-pointy tip to the soldering iron and heat it up to 350..400 Celsius, that'll be the body, the
    pinpoint will be just around 200, because of the greater heat loss.

- Carefully push the corner of the razor blade (with pliers, remember?) under pin 1 of the chip, from the inside, like if
    you wanted to cut its soldering outwards from the inside
- Gently but firmly press the pinpoint of the soldering iron to that pin of the chip, *only* to that one, for *3 seconds*,
    while trying to push the blade between the leg and the pcb
- If it doesn't melt in 3 seconds, then the iron is still too cold: turn it higher, but now *wait half a minute* to let
    the chip cool down before trying again
- If it succeeded, then get a fine flat screwdriver, push it between the blade and the pcb, and lift that de-soldered
    pin up slightly, like half a millimeter or so
- Congratulations 1 done, so only 27 pins to go :)
- Clean the pinpoint on a sponge, dip it a bit into some flux, apply some soldering alloy to cover it (as it has no
    special coating), and wipe that away again, just before proceeding to the next pin


## Re-soldering the chip

- Before all, fasten the chip to the clips of the soldering stand, and remove the old soldering from them
- If some pins are bent, then bend them back to the *exact* right place
- Put the chip down to a level surface, onto a slip of paper that fits right underneath the chip body
- With that flat screwdriver push gently all legs down to the surface (that's why the paper: they'll bend a paper
    width lower than the chip body)
- Put the chip onto the TSSOP28 breakout board and see if *all* pins match their pads *exactly*
- If not, then realign them until they do, you'll need a perfect match
- Put the chip aside, and fasten the breakout board to the clamps

- Still with the pinpoint tip, melt some soldering alloy onto the pads
- Use a regular 40-60% Pb-Sn alloy, those 'lead-free' concoctions still aren't eutectic, and if you ever want the alloy
    to go from liquid to solid *instantly*, then this is just that case
- Apply the *same amount* of alloy to each pad, as the more evenly the chip sits on these 'mounds' the easier it is to
    solder them there
- Nice 'wetting', 'flowing' solderings please, not just balls of tin that touch the pcb only at one point (if needed,
    practice this on some junk pcb first)

- Fasten the chip to the clamps, upside down, and also add some alloy to the bottoms of its pins
- Only a thin cover is needed, the rest is already on the pads
- Again: at most 3 seconds of heating, at least half a minute of cooling down
- Yes, 28 pins means about 15 minutes at least, but TSSOP28 isn't designed to take too much heat
- If you short-circuited two pins, don't panic. Wait the half minute and then insert the pinpoint between them, melt
    the alloy and swipe it away.

For a strong, reliable bond the alloy must stick to each surfaces, 'wet them', and applying an alloy coating ensures
this. If there is anything there that would prevent this (like oil from your skin), it's better to be revealed now when
you can deal with it pin by pin, than when 27 pins are already soldered to the board and the 28th just doesn't want to
stick there...

- Fasten the breakout board to the clamps, place the chip on it and align the pins to the pads
- Now that the pads have 'pillows' of alloy, it'll be harder, as the pins will want to slide between those 'pillows'
- Melt pin 1 to pad 1 by pressing the pinpoint of the iron downwards on it, remembering the 3-sec rule
- Check the aligment again (all pins), if not perfect, then melt pin1 and realign (repeat as needed :D )
- If you have a perfect fit, melt down pins 14, 15 and 28: the other corners. Now the chip sits secured on the board.
- Melt down each remaining pins, one by one, remembering the 3-sec rule and re-coating the pinpoint with alloy from
    time to time.
- Try not to short circuit the pins, it's much harder to clean it up now. Use a copper wick (or loosely spun multi-core
    wire) to drain away the alloy if all else fails.

- Grab a contact meter and ring out the soldering contacts by probing on the top of the chip pins and on the side
    connectors of the breakout board. Re-melt the failing pins, pressing gently from up down towards the board.

- De-solder all the remaining resistors and capacitors and the crystal (or just use new ones of the same values)
- Find places on the breakout board where to solder them on. Shameless tinkering creativity can help a lot here :)
- When applying those 27 Ohm resistors *between* the chip pins and the pins of the breakout board, just
    cut the pcb trace on the board, scrape away the mask around it, and solder the smd resistors just above the cut.
    (Sometimes some shameless tinkering destructivity can help as well :D )
- Only after all components are installed, should you solder in the pins of the breakout board, otherwise they'll
    always be in the way when fighting with those tiny smd thingies.


## Proofchecking the new circuit

- When you first connect it to your computer and it doesn't get recognised, don't despair but check for:
    - contact errors in the power lines: measure gnd-vcc on the tops of the chip pins
    - contact errors or short circuits at the crystal and its two capacitors: measure the frequency at pin27, it should be steady 12 MHz
    - contact errors of the USB signal lines: unplug the USB and measure resistance from connector pin to chip pin: it should be near 30 Ohms
    - short circuits between all above
- If the circuit is recognized as USB peripheral, check the signal pins:
    - launch a terminal emulator, choose no flow control, short rx with tx, type something and check if you got it back
    - attach some leds to the rest with 1k resistors towards the ground and launch the gpio test :)


## Details of my circuit

- The crystal is connected between pin28 and pin27
- Both pin28 and pin27 are coupled to the GND with 1nF capacitors
- The GND pins 7, 18 and 21 are internally connected, no need for external traces between them
- AGND (pin 25) and PLLTEST (pin 26) are connected to GND
- VO33 (pin 17) is connected to VDD325 (pin 4)
- USB D+ (green wire) is connected to pin 15 via a 27 Ohm resistor
- USB D- (white wire) is connected to pin 16 via a 27 Ohm resistor
- USB D+ (green wire) is connected to VO33 via a 1.5k resistor
- USB GND (black wire) is connected to GND (any of pins 7, 18, 21, 25, 26)
- USB 5V (red wire) is connected to VDD5 (pin 20)

- Two power smoothing capacitors of 1 uF and 33 nF can be connected in parallel between VCC and 5V, but it's not essential
- A 1 nF smoothing capacitor can be connected between VO33 and GND, but it's also not essential

[//]: # ( vim: set sw=4 ts=4 et: )
