# Seed function discovery for a raw RTL8720CF/AmebaZ2 Thumb XIP image.
# @category Gree

from java.math import BigInteger
from jarray import zeros
from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
base = toAddr(args[0] if len(args) else "0x9B000140")
memory = currentProgram.getMemory()
block = memory.getBlock(base)
if block is None or not block.isInitialized():
    raise RuntimeError("no initialized memory block at %s" % base)

program_context = currentProgram.getProgramContext()
tmode = currentProgram.getLanguage().getRegister("TMode")
if tmode is not None:
    program_context.setValue(tmode, block.getStart(), block.getEnd(), BigInteger.ONE)

size = int(block.getSize())
raw_java = zeros(size, "b")
read = memory.getBytes(block.getStart(), raw_java)
raw = bytearray((value & 0xFF) for value in raw_java[:read])
start = block.getStart().getOffset()
end = block.getEnd().getOffset()

seeds = set()
# The archived application begins with the 16-byte AmebaZIIRTL8710C marker.
if len(raw) > 0x10:
    seeds.add(start + 0x10)

# Function pointers are stored as odd Thumb addresses throughout Realtek and
# GREE dispatch/property tables. Scan both aligned and halfword-aligned words.
for offset in range(0, len(raw) - 3, 2):
    value = (raw[offset] | (raw[offset + 1] << 8) |
             (raw[offset + 2] << 16) | (raw[offset + 3] << 24))
    target = value & ~1
    if (value & 1) and start <= target <= end:
        seeds.add(target)

# Seed conventional Thumb and Thumb-2 function prologues. Auto-analysis will
# follow their direct branches/calls and reject candidates that do not decode.
for offset in range(0, len(raw) - 3, 2):
    if raw[offset + 1] == 0xB5:             # PUSH {..., LR}
        seeds.add(start + offset)
    elif raw[offset] == 0x2D and raw[offset + 1] == 0xE9:  # PUSH.W/STMDB SP!
        seeds.add(start + offset)

symbol_table = currentProgram.getSymbolTable()
seeded = 0
failed = 0
for value in sorted(seeds):
    address = toAddr(value)
    try:
        disassemble(address)
        if getInstructionAt(address) is None:
            failed += 1
            continue
        if getFunctionContaining(address) is None:
            createFunction(address, None)
        seeded += 1
    except Exception:
        failed += 1

first = toAddr(start + 0x10)
try:
    symbol_table.createLabel(first, "rtl8720cf_application_start", SourceType.USER_DEFINED)
except Exception:
    pass
try:
    symbol_table.addExternalEntryPoint(first)
except Exception:
    pass

print("RTL8720CF seed scan candidates=%d decoded=%d rejected=%d block=%s-%s" %
      (len(seeds), seeded, failed, block.getStart(), block.getEnd()))
