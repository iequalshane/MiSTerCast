# MiSTerCast
A general-purpose tool for streaming your Windows PC screen to your MiSTer through the Groovy_MiSTer core.

This is not a replacement for Groovy_Mame or other integrated emulators.	

Make sure you already have Groovy_Mame working well with Groovy_MiSTer before using MiSTerCast. A direct ethernet connection to your MiSTer is recommended.
https://github.com/lutechsource/MiSTerStuff/blob/main/GroovyMiSTer/mame_documentation.md
https://github.com/psakhis/Groovy_MiSTer

For audio, you will need to enable audio on the Groovy_MiSTer core.

## Known issues
- Frames may be dropped or doubled due to sync with video signal.
- At least 1-2 frames of latency.
- If the app crashes, you will need to restart your MiSTer and Groovy_MiSTer.
- Nothing over 720x480i is recommended at the moment due to throughput on MiSTer. This will improve soon.
- High refresh rate monitors are not supported due to frame times. Please change your monitor to ~60hz.

## Notes
The current pre-defined modelines are just for testing. You can add your own in modelines.dat.
It's best to use a refresh that matches your PC for better sync.
Find more modeline examples here: https://www.geocities.ws/podernixie/htpc/modes-en.html
