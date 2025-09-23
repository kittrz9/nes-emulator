# NES emulator
a simple NES emulator written in C<br>
<br>
the only currently implemented mappers are NROM, MMC1, UNROM, and MMC3, and MMC3 scanline stuff isn't fully implemented yet.<br>
<br>
the controls are defined in `src/input.c`, currently they are set to be<br>
`A - Z`<br>
`B - X`<br>
`DPad - Arrow Keys`<br>
`Start - Enter`<br>
`Select - Right Shift`<br>
<br>
## currently known issues
 - smb1 has issues with the hud scrolling due to weird issues with setting the scroll registers and probably innacurate sprite 0 hit stuff too
 - basically any MMC3 game that uses its scanline counter is broken
 - the pulse channel's sweep unit stuff (the stuff that makes the notes slide) is slightly broken, most noticable when going through a pipe or taking damage in smb1
<br>

## building
to build it all you should need to do is run `./build.sh` in this directory. it will download a version of SDL3 and compile that and then compile the emulator with it.<br>
<br>
there is no windows support and probably never will be.<br>
