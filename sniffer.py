# Calling XMC4500 functions from the Python 3 interpreter in gdb-multiarch
#
# Invoke with
# gdb-multiarch -batch -ex 'source sniffer.py' sniffer.elf
#

# set up GDB connection
gdb.execute("target extended-remote localhost:3333")
gdb.execute("set remote hardware-breakpoint-limit 6")
gdb.execute("set remote hardware-watchpoint-limit 4")

gdb.execute("load")  # uncomment to program the firmware into FLASH

# run xmc-sniffer.c application until breakpoint at start of ready()
gdb.execute("b ready")
gdb.execute("r")

# get references to some of the C functions and global variables
record     = gdb.parse_and_eval("record")
trace      = gdb.parse_and_eval("trace")
core_clock = gdb.parse_and_eval("core_clock")

print()

# acquire t seconds of trace data
t = 2
fs = int(core_clock)
print(f"Tick frequency: {fs/1e6} MHz")
print(f"Recording for up to {t} seconds ...")
n = int(record(t * 1000))
# dump into a file
fn = "trace"
print(f"Writing trace data to file '{fn}' ...")
with open(fn,"wb") as f:
    # sampling frequency in Hz as a bigendian 32-bit integer
    f.write((fs).to_bytes(4,byteorder='big'))
    # number of trace records as a bigendian 32-bit integer
    f.write((n).to_bytes(4,byteorder='big'))
    # each trace record as a bigendian unsigned 32-bit integer
    # (see sniffer.c comments for details)
    for i in range(0,n):
        f.write(int(trace[i]).to_bytes(4,byteorder='big'))
    f.close()
print(f"{n} records written.")
