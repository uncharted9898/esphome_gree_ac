# Add a companion RTL8720CF image section to the current Ghidra program.
# @category Gree

from java.io import File, FileInputStream
from java.math import BigInteger

args = getScriptArgs()
if len(args) < 2:
    raise RuntimeError(
        "usage: load_rtl8720cf_companion.py <raw-file> <base-address> [block-name] [execute]"
    )

path = args[0]
base = toAddr(args[1])
name = args[2] if len(args) > 2 else "rtl8720cf_companion"
execute = len(args) <= 3 or args[3].lower() not in ("0", "false", "no")
source = File(path)
if not source.isFile():
    raise RuntimeError("companion file does not exist: %s" % path)

memory = currentProgram.getMemory()
if memory.getBlock(base) is not None:
    raise RuntimeError("memory block already exists at %s" % base)

stream = FileInputStream(source)
try:
    block = memory.createInitializedBlock(
        name, base, stream, source.length(), monitor, False
    )
finally:
    stream.close()
block.setRead(True)
block.setWrite(False)
block.setExecute(execute)

if execute:
    tmode = currentProgram.getLanguage().getRegister("TMode")
    if tmode is not None:
        currentProgram.getProgramContext().setValue(
            tmode, block.getStart(), block.getEnd(), BigInteger.ONE
        )

print("Loaded %s at %s-%s execute=%s" %
      (path, block.getStart(), block.getEnd(), execute))
