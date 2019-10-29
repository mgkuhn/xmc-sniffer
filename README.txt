XMC-Sniffer is (the beginnings of) a basic low-cost logic analyzer
running on an Infineon XMC4000-series microcontroller, in particular
at the moment the Infineon XMC4500 Relax Lite Kit.

The included XMC firmware observes eight digital inputs (by default
PORT1 pins 8-15, weakly pulled high) with a sampling frequency of at
least 1 MHz, and records for each change of input bits there a 32-bit
record that includes a 23-bit timestamp. If there is no change on the
input, it appends one 32-bit record every week. LED P1.0 indicates
that recording is in progress and LED P1.1 blinks for 70 ms after each
change of input status.

The firmware is currently operated via the remote-debug interface of
gdb and openocd, which are used to both start the recording and
retrieve the resulting records.

Installation (tested on on Ubuntu Linux 18.04):

$ sudo apt-get gcc-arm-none-eabi binutils-arm-none-eabi gdb-multiarch openocd
$ make ./xmclib   # to download Infineon's XMCLib CMSIS library
$ make

Run:

$ make ocd   # in a separate terminal to start the openocd server

$ make sniff

This currently records for 20 seconds, but that time (and the output
filename "trace") can easily be changed in sniffer.py.

Writing any tool to analyze or visualize the resulting "trace" file is
currently still left as an exercise for the reader.

Author: Markus Kuhn
