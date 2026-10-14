# Hardware assumptions and bench check

The configured hardware is an UNO R4 WiFi, an unmodified Arduino Motor Shield Rev3, and a
Yellowtec m!ka red/white on-air lamp. The reported supply is nominally 12 V, 1 A and plugs
into the UNO's barrel connector; the lamp connects to the shield's output terminals.
The adapter's exact label, polarity, loaded voltage and lamp current have not been verified here.

| Connection | Default |
| --- | --- |
| Shield output | Channel B screw terminals |
| PWM / enable | D11, 12-bit duty, approximately 490 Hz |
| Direction | D13: LOW red, HIGH white |
| Brake | D8: HIGH when off, LOW when lit |
| Vin Connect | Still intact; shield and Arduino VIN rails are linked |

With the intact jumper, barrel-jack power also supplies the shield's bridge through VIN.
The lamp draws power through this supply path, not through a microcontroller GPIO or the
UNO's 5 V regulator. Arduino documents barrel-jack power as an option for this shield and
specifies a 2.1 mm center-positive connector. The shield's 2 A per-channel rating does not
establish a continuous-current rating for the entire adapter, connector and board power path.

The [Motor Shield Rev3](https://store.arduino.cc/products/arduino-motor-shield-rev3) is an
L298P bridge. Its recommended external supply range is 7–12 V and its VIN terminal absolute
limit is 18 V. The [UNO R4 WiFi](https://store.arduino.cc/products/uno-r4-wifi) supports 6–24 V
on VIN, but that does not increase the shield's limits. The shield's documentation recommends
separating the VIN connection when supplying the shield above 9 V; this rewrite neither
changes that jumper nor increases the supply. Cutting it with this barrel-only supply would
disconnect the shield's motor power; a separately powered shield requires deliberate rewiring.

The [Yellowtec booklet, printed page 4](https://www.yellowtec.com/fileadmin/mika/mika_BB_v7_email.pdf)
specifies **12–24 V DC at the on-air light**, with polarity selecting red/white. It does not
specify a PWM dimming frequency or minimum pulse width there. The [L298 datasheet](https://www.st.com/resource/en/datasheet/l298.pdf)
shows that the bridge has a current-dependent voltage drop. Consequently, a 12 V adapter
does not establish that 12 V reaches the lamp. The adapter's 1 A marking is its capacity,
not a measurement of the lamp's current. Do not raise this shield's supply to 24 V simply
because the lamp and UNO individually accept it.

The firmware retains the existing PWM carrier and the observed minimum duty. It does not
assume the lamp tolerates a higher PWM frequency or that extra numerical resolution fixes
electrical flicker. Separate red and white floors allow calibration after testing.
At 12-bit resolution, 4095 means 100% duty, exactly as 255 does at 8-bit resolution. It does
not raise the voltage or the peak current. Mode changes now reach that peak immediately.

Startup first disables the bridge enable, then applies the brake before initializing PWM.
Pin levels are set after `pinMode(OUTPUT)` because the pinned Renesas core clears the output
latch when configuring an output. This also makes the initial physical polarity match the
driver's cached state. Reset/bootloader behavior before the sketch runs still belongs to
the board and shield hardware; host tests cannot certify it.

## First physical test

1. Check the adapter label for regulated 12 V DC with center-positive polarity. Unplug the
   barrel adapter for the first upload and power the UNO over USB. After uploading, power
   down before changing any wiring, reconnect the normal supply and confirm the lamp starts
   off. Open a USB connection at 115200 baud for a brief supervised test.
2. Send `red`, `white`, and `off`. Check the actual colors and full output. If reversed,
   set `TALLY_RED_DIRECTION_HIGH` in the ignored local configuration and rebuild.
3. Send `hot` and `idle` from off and during the dimmest part of the opposite mode. Each
   change should appear bright as soon as the command is processed; off should extinguish it.
4. Repeat the same breathing command mid-cycle. The phase should continue. Alternate USB
   and HTTP commands; the latest valid command should determine both status and output.
5. Leave Wi-Fi unavailable or disconnect the access point. Check USB changes and breathing
   while reconnects happen. Read `status` and record `max_service_gap_ms`. Compare a
   `-UsbOnly` build if network pauses are still visible.
6. Watch several low-brightness cycles in both colors. If flicker remains, measure the
   adapter and voltage across channel B at **steady full output** in each polarity. A
   multimeter's average in a breathing/PWM mode cannot establish the on-pulse voltage.
   An oscilloscope is needed to check pulse shape and actual PWM timing.
7. Run a longer session, including slow HTTP senders, USB reconnects and rapid mode changes.
   Record any remaining flicker, stalls or missing acknowledgements before adjusting floors
   or the modem timeout. Do not lower the empirical floor solely because 12-bit values exist.

Host tests and a successful target compilation do not replace these checks. No physical
lamp measurements or board upload are performed by the test/build scripts.
