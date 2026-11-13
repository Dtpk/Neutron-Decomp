Some notes this was a challenge currently some visual glitches but a lot of optimizations audio now works but it has a quirk to prevent full rewrite to include streaming if you do custom audio keep it small like the audio it ships with the text rendering was redone to save half the frame rate which was a huge minor change. So on other ports I may consider doing the same tweak.

Not all this code is mine this is more or less fully worked out with a llm. Sadly xbox opengl is kinda a hack job I had a hard time trying to figure out. I think there might have been one game that was used as refrence that I based this on and needed in order to get this to function so a big chunk of credit goes to the ones who actually made a implementation for the xbox that works as well as it does.

It is not perfect culling was a issue and had a few toggles that needed to be forced to prevent that the behavior was odd. As far as I am aware this is playable till the end again without sounds right now and it can save to the xbox save directory just fine.

Custom maps can be loaded in e partition under udata you should find a neutron folder drag and drop you save or custom map to play it. Also split screen random spawn logic is in so it is now on parity with non online multiplayer builds.

I only tested 3 player multiplayer so besides that frame rate seemed good if adding a 4th breaks it I will find out then.

This is best with blueretro using black and white buttons which map to the bumpers has been great. But it just standard controls if not using the bumpers on a stock xbox remote.
