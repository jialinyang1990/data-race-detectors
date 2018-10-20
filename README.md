Data race detectors built on top of Maple (https://github.com/jieyu/maple)

### Detectors
    \src\pintool\eraser         Eraser          [Savage S et al., 1997]
    \src\pintool\djit           Djit            [Pozniansky E, Schuster A, 2003]
    \src\pintool\fasttrack      FastTrack       [Flanagan C, Freund SN, 2009]
    \src\pintool\acculock       AccuLock        [Xie X, Xue J, 2011]
    \src\pintool\multilock      MultiLock       [Xie X, Xue J, Zhang J, 2013]
    \src\pintool\histlock       HistLock        [Yang J, Yang C, Chan WK, 2016]
    \src\pintool\histlockplus   HistLock+       [Yang J, Jiang B, Chan WK, 2018]

### Supported OS

* Redhat Enterprise Linux 5.4 (x86\_64)
* Ubuntu Desktop 10.10 (x86\_64)
* Ubuntu Desktop 11.04 (x86\_64)
* Ubuntu Desktop 12.04 (x86\_64)

### Software Dependencies

* GNU make, version 3.81 or higher
* Python, version 2.4.3 or higher
* [Google protobuf](http://code.google.com/p/protobuf/), version 2.4.1
* [Pin](http://www.pintool.org/), revision 62732 or higher

### Build

First, you need to set two environment variables.

    $ export PIN_HOME=/path/to/pin/home
    $ export PROTOBUF_HOME=/path/to/protobuf/home

Then, you need to modify the build file `package.mk` so that the top level makefile can correctly build the analysis tool.

    $ cd <maple_home>/src/pintool
    $ vi package.mk
    1 srcs += ... pintool/{detector_name}.cpp
    2
    3 pintools += {detector_name}.so
    4
    5 {detector_name}_objs := ... pintool/{detector_name}.o $(core_objs)

Now, you can build the detector using make.

    $ cd <maple_home>
    $ make
    $ make compiletype=release

Once the building finishes, two directories can be found in the source directory.

    $ cd <maple_home>
    $ ls
    build-debug build-release ...

### Usage

    $ cd <maple_home>
    $ pin -t build-release/{detector_name}.so -- {program under test}
    $ race display race > dynamic_race.txt
    $ race display static_race > static_race.txt
    $ race usage
