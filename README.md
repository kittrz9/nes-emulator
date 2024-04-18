# NES emulator
a simple NES emulator written in C<br>
<br>
it only has support for NROM, MMC1, and UNROM games, and has a lot of stuff that is unimplemented and buggy<br>
<br>
it's also only been tested with a few games. galaga, tetris, and megaman seem to work fine but super mario bros gets stuck on the title screen due to not having the sprite 0 hit implemented<br>
<br>
the framerate is also uncapped lmao<br>
<hr>

## building
to build it all you should need to do is run `./build.sh` in this directory. it will download a version of SDL2 and compile that and then compile the emulator with it.<br>
<br>
there is no windows support and probably never will be.<br>
