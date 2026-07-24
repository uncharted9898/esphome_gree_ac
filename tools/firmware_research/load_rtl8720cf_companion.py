# Add the companion RTL8720CF XIP section to the current Ghidra program.
# @category Gree

from java.io import File, FileInputStream
from java.math import BigInteger

args = getScriptArgs()
if len(args) != 2:
    raise RuntimeError("usage: load_rtl8720cf_companion.py <raw-file> <base-address>")

path = args[0]
base = toAddr(args[1])
source = File(path)
if not source.isFile():
    raise RuntimeError("companion XIP file does not exist: %s" % path)

memory = currentProgram.getMemory()
if memory.getBlock(base) is not None:
    raise RuntimeError("memory block already exists at %s" % base)

stream = FileInputStream(source)
try:
    block = memory.createInitializedBlock(
        "rtl8720cf_companion_xip",
        base,
        stream,
        source.length(),
        monitor,
        False,
    )
finally:
    stream.close()

block.setRead(True)
block.setWrite(False)
block.setExecute(True)

program_context = currentProgram.getProgramContext()
tmode = currentProgram.getLanguage().getRegister("TMode")
if tmode is not None:
    program_context.setValue(tmode, block.getStart(), block.getEnd(), BigInteger.ONE)

print("Loaded companion RTL8720CF XIP block %s-%s from %s" %
      (block.getStart(), block.getEnd(), path))
