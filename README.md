# Path Set plugin for VCV Rack

The Path Set is an eclectic collection of bespoke modules for [VCV Rack](https://vcvrack.com/). The modules are:

* [Astro Vibe](#astro-vibe) - Three stereo oscillators or LFOs with random waveforms. Explore a universe of unique waveforms.
* [Ice Tray](#ice-tray) - Speed shifter and tape delay with selective memory. Perform into this and it will never forget your mistakes.
* [Shifty](#shifty) - Gate shift register with controllable delays. Create generative gate sequences from a simple clock.

There are also [Examples Patches](https://github.com/patheros/PathSetModules/tree/main/examples) for each of these modules. Note they use [other plugins](#other-plugins).

## Astro Vibe

![Image of IceTray module](https://github.com/patheros/PathSetModules/blob/main/images/AstroVibe.png)

Astro Vibe is three planetary obiters that can operate as VCOs or LFOs. Each orbiter has a wide range of controllable settings as well as internal planetary signature that affects the waveform. There are over three septillion planetary signatures and once you leave one your unlikely to ever find it again.

### Planet

Each orbiter generates a unique planetary signature when the module is created and when either the `New Planet Button or Trigger` is activated. The planetary signature affects the output waveform giving it a unique signature. For the Notes waveform mode each signature creates a unique sequence of between 2 and 22 different notes to play. In the crunchy waveform mode it affects the overall timbre of the sound. There are over three septillion signatures and no way to return to one once you leave. Currently the signatures do not persist when the patch is saved.

### Clock and Freq

Each orbiter has a `Clock` input which advances the notes in the Notes waveform mode. It does nothing in the Tones waveform mode. The clocks on the 2nd and 3rd orbiter are normalized to the previous orbiter, meaning you only have to connect a clock to the 1st orbiter to get all 3 going.

Each orbiter also has a `Frequency Knob` and `1V/Oct Input`. These two control the overall tone of the sound in the Audible mode and the overall speed of the LFO in the LFO mode. The 1V/Oct input is also normalized between the orbiters.

### Engine and Waveform

The central switches control the `Engine` and `Waveform` of each Orbiter. Each switch is togglable to change the mode and also controllable with the corresponding input. If the input is high (above 5 volts) the toggle is inverted. If its low (bellow 0 volts) the toggle acts as normal. If the input is between 0 and 5 volts, the toggle flips back and forth and is proportionally one way or the other depending on what the input voltage is.

When the `Engine` mode is `Atomic` the sound more pure and smaller. When the mode is `Black Hole` the sound is buzzier and louder.

When the `Waveform` mode is on `Notes`the sound is melodic and plays a sequence with the clock. When on `Tones` the sound is always on and can be more crunchy. 

### Warping

The `Space Input`, `Spin Input` and `Warp Knob`all adjust the timbre of the sound as well as the panning between the left and right channels. The overall effect is more pronounced on the Tones waveform.

The Space and Spin Inputs are double normalized. If the previous orbiter is set to LFO mode, then the left and right channels are routed to the Space and Spin respectively. If not, then the previous orbiters space and spin inputs are carried forward.

### LFO vs Audio

Each orbiter has an `Speed` toggle that switches between an `Audible` mode and a `LFO` mode. In the Audible mode the output is also added to the master output at the top of the Module. In the LFO mode the output is normalized into the next orbiter's Space and Spin.

### Examples
* [AstroVibe_Example1](https://github.com/patheros/PathSetModules/blob/main/examples/AstroVibe_Example1.vcv) - Shows all 4 possible combinations of Engine and Waveform when used in audio output. Also demonstrates the variety the planetary signature provides the oscillator. 
* [AstroVibe_Example2](https://github.com/patheros/PathSetModules/blob/main/examples/AstroVibe_Example2.vcv) - Polyphonic example also using one of the obiters as an LFO and the internal routing.
* [AstroVibe_Example3](https://github.com/patheros/PathSetModules/blob/main/examples/AstroVibe_Example3.vcv) - More complete atmospheric patch showing off two AstroVibes.
* Note examples use [other plugins](#other-plugins).

## Ice Tray

![Image of IceTray module](https://github.com/patheros/PathSetModules/blob/main/images/IceTray.png)

Ice Tray is a speed shifter and six tape delays, called ice cubes. Each cube can solidify, capturing what ever was last recorded into it for an indefinite amount of time.

### Ice Cubes

The central section of the module displays the six `Ice Cubes`. The color of the large light indicates the`Solidity` of the ice cube:

* **Bright Blue** - Can be recorded to or played from
* **Dark Blue** - Can only be played from
* **Black** - Can't be accessed, but still remembers its last recording.

You can press the light to change its solidity. The large `Frozen Percent` knob also controls how likely a cube's solidity is to change over time. Crank it all the way up to 100% and the solidity won't change, unless you manually do so. 

The small light on the cube indicates which ice cube is being recorded (red) to and which is being played back (green). An ice cube can't be both at the same time.

Note the recordings on the cubes currently do not persist when the patch is saved. The recordings can also be cleared through the `Clear Cubes` option in the contextual menu.

### Clocks

Ice Tray doesn't need any clocks to work but you can connect either or both of its clocks to signals to get more control over the rhythm of the output.

When the `Record Clock` is NOT connected, the`Record Length` knob sets the maximum number of seconds each cube is recorded too before switching to a new cube. When `Record Clock` IS connected, the recording cube will switch when the clock is received, or after 10 seconds. Note all times assume 44.1kHz.

Normally the playback cube will finish its recording before switching to a new cube, but you can end this early by sending a trigger to the `Playblack Clock`.

The `Playback Clock Resets Position` toggle controls where the playback starts on each cube. When on the playback starts at the start of each cube, but when off the position is not reset, so wherever the last cube ended the next cube starts.

### Speed Shift

The two large dials on the left control the speed at which the input signal is recorded. They do not control the pitch though, that is maintained regardless of the playback speed.

The `Numerator`  is divided by the `Denominator` to compute the final speed. Both the numerator and denominator can be modulated using the CV inputs and CV scalars. The modulated value is added to that on the large dial. The numerator and denominator are each rounded to the nearest whole number before computing the final speed.

### Patterns and Repeats

The `Repeat` knob controls how many times a single ice cube is repeated before another one is selected. If a CV is connected bellow the know then the knob scales that voltage.

The `Pattern` knob controls the order in which the ice cubes are are played back in. If the knob is turned to the left, the cubes are randomly skipped in increasing large steps leading to a random pattern each time. If the knob is turned to the order of the cubes is shuffled but consistent each time though with the caveat that cubes that are black, or being recorded to are skipped. 

### Feedback

The `Feedback` knob and corresponding CVs at the bottom of Ice Tray feed the output back into the recording cube. Unlike the input signal, the feedback signal is both speed up and pitch shifted by the speed controls. This gives the internal feedback a unique effect that an external source of feedback couldn't replicate.

### Examples
* [IceTray_Example1](https://github.com/patheros/PathSetModules/blob/main/examples/IceTray_Example1.vcv) - Mini Tutorial of IceTray with a simple voice and lots of notes.
* [IceTray_Example2](https://github.com/patheros/PathSetModules/blob/main/examples/IceTray_Example2.vcv) - Example of a single voice through IceTray with modulation.
* [IceTray_Example3](https://github.com/patheros/PathSetModules/blob/main/examples/IceTray_Example3.vcv) - IceTray with another delay and reverb to create more variety in the sound.

Note examples use [other plugins](#other-plugins).

## Shifty

![Image of IceTray module](https://github.com/patheros/PathSetModules/blob/main/images/Shifty.png)

Shifty is a shift register for gates / triggers with controllable lookback / delay. Shifty lets you take a simple gate sequence  and create seven varied gate outputs.

The easiest way to get started with Shifty is connect a reasonably fast clock signal to the `Clock` input. Every 8th clock you'll see the the top blue light turn on and then shift down once with each clock.

You can connect another input to `Trigger` or adjust the `Clock Divider` to change how frequently the first blue light turns on.

Connect to the seven `Output Gates` to tap into those delayed clock signals.

### Delay

By default Shifty is set up to delay each row by one beat / clock. This can be changed by adjusting the `Ramp` Knob at the top of Shifty. Turn `Ramp` all to way to the left and every row hits at once. Turn `Ramp` all the way to the right and it takes two beats for each row to hit.

Bellow the ramp knob are the `Delay CV inputs and knobs`. These add to the ramp value allowing you adjust the delay up or down for each row.  The `Delay` input is normalized to white noise allowing you to just use the knobs to add randomness. The noise ranges from 0 to 10 volts and at 10 volts the row is delayed an extra 16 beats.

Finally you can add stability to to the delays with the `Sample & Hold` knob. This knob controls how likely each row's total delay value is to be held, indicated by the red light in the row. Turning the knob all the way to the right effectively freezes the current delay pattern. Note the sample and hold chance is triggered every time the clock divider triggers, regardless of whether or not the trigger input is connected. Currently the sample and hold values do not persisted when the patch is saved.

### Echo and Mute

Between the shift register and the outputs are the `Echo` and `Mute` knobs. Use them to add more or less hits on each row. By default they don't do anything and you can see you can see the pink and orange lights match the blue light on the same row.

Turn the`Echo` knob up to add up to 3 extra hits to that row's gate. Each of the extra hits follow slightly different timings depending on how far the `Echo` knob is turned. This means there is a lot of variety of patterns you can get as you fine-tune the knob. The echo timing is also influenced by the incoming `Delay` value, allowing for more variety or stability depending on how the delay side is configured.

The `Mute` knob suppresses hits. If you have a frequent trigger or a lot of echos it can be helpful mute some of the hits. The mute knob pans through 693 different 24 beat patterns ranging from all beats passing to no beats passing. The mute pattern is further modified by the delay values so that you crank the clock divider all the way down to 1 you can still use the mute knob to get interesting and changing patterns.

### Examples
* [Shifty_Example1](https://github.com/patheros/PathSetModules/blob/main/examples/Shifty_Example1.vcv) - Uses the first two gates of Shift to drive a drum beat out of two [Palettes](https://library.vcvrack.com/Atelier/AtelierPalette) and the remaining gates are used as CVs on the VCOs and effects.
* [Shifty_Example2](https://github.com/patheros/PathSetModules/blob/main/examples/Shifty_Example2.vcv) - 5 Shiftys working to drive 3 different voices. Each voice showing off different ways to use Shifty.
* [Shifty_Example3](https://github.com/patheros/PathSetModules/blob/main/examples/Shifty_Example3.vcv) - Shifty outputs are merged to create a polyphonic gate. Along with a polyphonic VCO this creates melodic variety with a high degree of configurability.
* [Shifty_Example4](https://github.com/patheros/PathSetModules/blob/main/examples/Shifty_Example4.vcv) - More examples of melodic patterns created by Shifty and chaining shifty together.
* [Shifty_Example5](https://github.com/patheros/PathSetModules/blob/main/examples/Shifty_Example5.vcv) - Using LFOs to mores slowly change Shifty's pattern over time. 

Note examples use [other plugins](#other-plugins).

## License
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

All **graphics** in the `res` and `res_source` directories are copyright © [Andrew Hanson](https://github.com/patheros) and licensed under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/).

## Other Plugins
The [examples patches](https://github.com/patheros/PathSetModules/tree/main/examples) in this repository use these other plugins. They are all free but you will need to subscribe to them in order to load the examples.

1. [Alright Devices](https://library.vcvrack.com/AlrightDevices)
1. [Arron Static](https://library.vcvrack.com/AaronStatic)
1. [Befaco](https://library.vcvrack.com/Befaco)
1. [Bogaudio](https://library.vcvrack.com/Bogaudio)
1. [Count Modula](https://library.vcvrack.com/CountModula)
1. [Impromptu](https://library.vcvrack.com/ImpromptuModular)
1. [JW-Modules](https://library.vcvrack.com/JW-Modules)
1. [Mind Meld](https://library.vcvrack.com/MindMeldModular)
1. [Palette](https://library.vcvrack.com/Atelier/AtelierPalette)
1. [Vult Free](https://library.vcvrack.com/VultModulesFree)
