<!-- This file uses Github Flavored Markdown (GFM) format. -->

# Greybus Simulator (gbsim)

A tool which simulates an arbitrary set
of Ara modules plugged into Greybus.
Only work only support TCP/IP as Greybus transport layer.

Provided under BSD license. See *LICENSE* for details.

## Quick Start

This code depends on header files present in the "greybus" source
directory.  The location of this directory is defined in the *GBDIR*
environment variable.

To just build gbsim, do this:
```
export GBDIR=".../path/to/greybus"
./autogen.sh
./configure
make
```

## Install

Set the environment variable *GBDIR* to point at the kernel greybus
(https://github.com/gregkh/greybus) directory as the simulator shares
headers with the kernel code.

`export GBDIR=".../path/to/greybus"`

Build it:

gbsim has the following dependencies:

* libsoc (https://github.com/jackmitch/libsoc)
* https://github.com/lathiat/avahi.git

It also assumes the *GBDIR* environment variable has been set.
```
cd /path/to/gbsim
./autogen.sh
./configure
make
make install
```

If you would like to cross-compile the simulator, you can optionally
specify the prefix used for compilation tools at configure time.
For example:
```
./configure --host=arm-linux-gnueabi
```
## Run

Now start the simulator:

```
gbsim -h /path/to -v
```

Where */path/to* is the base directory containing the
directory *hotplug-modules*

gbsim supports the following option flags:

* -b: enable the BeagleBone Black hardware backend
* -h: hotplug base directory
* -i: i2c adapter (if BBB hardware backend is enabled)
* -v: enable verbose output

### Using the simulator

At this point, it's possible to hot plug/unplug modules by simply copying or
removing a conformant manifest blob file in the /path/to/hotplug-module
directory. Manifest blob files can be created using the Manifesto tool
found at https://github.com/projectara/manifesto. Using the Simple I2C Module as
an example, a module can be inserted as follows:

`cp /foo/bar/simple-i2c-module.mnfb /path/to/hotplug-module/simple-i2c-module.mnfb`

To work, you will need to run on another computer gbridge application.
Form here, gbridge will detect gbsim as a Greybus module and hotplug it.
Please see instructions at https://github.com/anobli/gbridge
