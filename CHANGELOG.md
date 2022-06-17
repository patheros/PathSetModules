
# Change Log

## v2.3.1
* IceTray
  * Fixed crash when completely froze, i.e. black.
* Nudge
  * Added Direct Link to Manual.
* Shifty
  * Right Click Menu now won't let you add an expander if there is already one attached.


## v2.3.0

* Nudge - New Module!
  * Generate five modulation signals that can be nudged to new values at the press of a button.
* Shifty
  * Now has an Expander
  * Fixed bug causing delay the Ramp parameter to be wrong. Now correctly delays by 1 clock per row when ramp is set to 1.
* AstroVibe
  * Second and Third Input Trigger now works [#1](https://github.com/patheros/PathSetModules/issues/1)
* GlassPane
  * Added more Voltage Ranges [#2](https://github.com/patheros/PathSetModules/issues/2)


## v2.2.0

* GlassPane - New Module!
  * Patchable, branchable, network sequencer. Create a cascading intricate self modulating sequence.


## v2.1.1

* IceTray
  * Removed all the weird clicking (that I could find)
  * Now correctly saves recordings when patch is saved (wasn't working correctly before)


## v2.1.0

* All Modules now save and load their full state
* AstroVibe
  * New Gain knob for each orbiter  
  * Internal Routing can be disabled from the contextual menu
  * Frequency Knob has a larger range in LFO mode
* IceTray
  * Now Stereo 
  * CPU saving option in contextual menu (disables pitch correction)
  * Fixed typos on Panel
* Shifty 
  * New Internal Clock with adjustable BPM 
  * Initialize now clears the shift register