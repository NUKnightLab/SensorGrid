# Knight Lab SensorGridPM

Wireless particulate matter monitoring over LoRa networks

This is a version of the SensorGrid project that is specific to monitoring
particulate matter data (PM 2.5 and PM 10) and collecting that data over a
LoRa wireless network.


## Development

This code is structured for development in PlatformIO with its entry point set
to enable Arduino IDE compilation as well. See below for information about
compiling with the Arduino IDE.

### Dependencies

See the `lib_deps` section of the `platformio.ini` file

### Arduino IDE

Development is mostly done on PlatformIO (https://platformio.org/) but we do
occasional checks to ensure deployment on Arduino IDE. It should just work.

### Code Style

Please lint with cpplint before submitting code changes:

See this project's `CPPLINT.cfg` file for details about flags used for linting.
```
cpplint <SubProject>/*
```

We are making an effort to move toward the Google C++ style guide:
https://google.github.io/styleguide/cppguide.html

with some notable exceptions:

#### include subdirs not required

The requirement for specifying subdirs adds unnecessary complexity to project
maintenance. We use a relatively flat code structure within a given project,
thus includes will either be in the form `"file.h"` where file.h is in the
same directory or `<Library.h>` where Library is accessible on the compile path.

As a result, please lint with `--filter=-build/include_subdir`


#### naming conventions

 * Variables should be `snake_case`
 * Functions and methods should be `camelCase` (Not PascalCase)


## Troubleshooting

### The device goes into time-setting mode without pressing the time set button

The battery or solar subsystem must be plugged in for the device to start up in the standard runtime mode.