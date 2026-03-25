# haptic-device
Code for the FRI haptic device project

**NOTE:** When launching the program, you may need to calibrate the device by pushing it in and out until it is recognized.

## OPTIONS
Specify the # of atoms at launch like so:
```
./haptic-device 38
```
If you don't pass in anything, the default is five.

You can also read in an existing configuration:
```
./haptic-device example.con
```
Make sure the .con file is in ../resources/data.

Choose the potential energy surface by adding a second argument:
```
./haptic-device 25 morse
```
The default is Lennard-Jones(lj). Other options are morse and ase.

## Build Instructions

### Windows

Windows development temporarily halted (unable to compile on Visual Studio).
Check WINDOWS.md for details on installation.

### Linux
1. Download or clone a CHAI3D tree and build it first
   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j"$(nproc)"
   ```
2. Run the following commands
   ```
   sudo add-apt-repository universe
   sudo apt update
   ```
3. Install the required packages using the command
    ```
    sudo apt-get install libusb-1.0-0-dev libasound2-dev freeglut3-dev xorg-dev python3-dev
    ```
4. Clone this repo into the CHAI3D directory so it sits next to `src`, `examples`, and `build`
5. Create the directory `data` in `bin/resources` and move the file `global_minima.txt` there
6. Run `make` in the `haptic-device` folder
7. Your directory structure should look like so:
<pre>
chai3d/
├── bin
│   ├── lin-x86_64
│   ├── resources
│       └── <b>data</b>
│           └── <b>global_minima.txt</b>
│
├── build
└── <b>haptic-device</b>
    ├── <b>build</b>
    ├── <b>CMakeLists.txt</b>
    ├── <b>LJ.cpp</b>
    ├── <b>Makefile</b>
    └── <b>README.md</b>
</pre>

At this point, the software should run with mouse and keyboard. The following steps are for setting up the haptic device
8. The binary will be written to `bin/lin-x86_64/haptic-device`
9. You may to change lines involving the relative file path
```c++
bool fileload = texture->loadFromFile(RESOURCE_PATH("../resources/images/spheremap-3.jpg"));
```
to the absolute file path.

10. Run the following commands while in the CHAI3D root:
  * `sudo cp ./externals/DHD/doc/linux/51-forcedimension.rules /etc/udev/rules.d`
  * `sudo udevadm control --reload-rules && udevadm trigger`

### MacOS
1. Download the latest release of CHAI3D for Mac OS X from [chai3d](http://www.chai3d.org/download/releases)
2. Make sure you have XCode downloaded, and follow instructions from the file entitled "getting-started.html" located in the doc folder of chai3d
3. Copy over all of the files from haptic-device into one of the CHAI3D examples, and rename LJ.cpp to to the same name of the .cpp file already in the CHAI3D example, such as "01-mydevice.cpp". If you're getting an error, make sure that you don't have a dupicate LJ.cpp file.
4. Run the example.


## Reference
The textbook is too big to upload so here's the link: http://www.charleshouserjr.com/Cplus2.pdf


## Notes

### Controls
* The buttons are labeled 0-3, starting at the center and going clockwise for user switches
* Button naming convention in LJ-test.cpp (example = name in LJ-test.cp
* p)
    * button 0 = button
        * turns off forces while pressed
    * button 1 = button2
        * this button changes the current atom being used
    * button3 = freebutton
        * also does nothing
    * button2  = button3
        * this changes the camera position
* Keyboard hotkeys:
    * `q` or `ESC`
        * quit program
    * `f`
        * toggle fullscreen
    * `u`
        * unanchor all atoms
    * `s`
        * screenshot atomic configuration without graph
    * `SPACE`
        * freeze atom movement
    * `c`
        * save configuration to .con file
    * `a`
        * anchor all atoms     
    * `ARROW KEYS`
        * move camera
    * `[` and `]`
        * zoom in/out
    * `r`
        * reset camera
    * `CTRL`
        * toggle help panel
