# NES emulator
a simple NES emulator written in C<br>
<br>
the only currently implemented mappers are NROM, MMC1, UNROM, MMC3, MMC2, ANROM, and Sunsoft 5B (though the 5B's expanded audio only has enough implemented for Gimmick).<br>
<br>
the controls are defined in `src/input.c`, currently they are set to be<br>
`A - Z`<br>
`B - X`<br>
`DPad - Arrow Keys`<br>
`Start - Enter`<br>
`Select - Right Shift`<br>
<br>
## currently known issues
 - occasionally crackly audio
 - battletoads crashes in the intro, probably due to inaccurate timing
<br>

## building
to build it all you should need to do is run `./build.sh` in this directory. it will download a version of SDL3 and compile that and then compile the emulator with it.<br>
<br>
there is no windows support and probably never will be.<br>
