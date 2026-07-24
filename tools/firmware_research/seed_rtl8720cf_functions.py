# Seed function discovery for a raw RTL8720CF/AmebaZ2 Thumb XIP section.
# @category Gree

from jarray import zeros
from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
base = toAddr(args[0] if len(args) else "0x9B000140")
memory = currentProgram.getMemory()
block = memory.getBlock(base)
if block is None or not block.isInitialized():
    raise RuntimeError("no initialized memory block at %s" % base)

size = int(block.getSize())
raw_java = zeros(size, "b")
read = memory.getBytes(block.getStart(), raw_java)
raw = bytearray((value & 0xFF) for value in raw_java[:read])
start = block.getStart().getOffset()
end = block.getEnd().getOffset()

seeds = set()
application_marker = bytearray(ord(value) for value in "AmebaZIIRTL8710C")
if len(raw) > 0x10 and raw[:16] == application_marker:
    seeds.add(start + 0x10)

# Function pointers are stored as odd Thumb addresses throughout the Realtek
# runtime and GREE dispatch/property tables. Scan aligned and halfword-aligned
# words so tables with 16-bit members are not missed.
for offset in range(0, len(raw) - 3, 2):
    value = (raw[offset] | (raw[offset + 1] << 8) |
             (raw[offset + 2] << 16) | (raw[offset + 3] << 24))
    target = value & ~1
    if (value & 1) and start <= target <= end:
        seeds.add(target)

# Seed conventional Thumb and Thumb-2 function prologues. Auto-analysis follows
# their direct calls and rejects candidates that do not decode as instructions.
for offset in range(0, len(raw) - 3, 2):
    if raw[offset + 1] == 0xB5:  # PUSH {..., LR}
        seeds.add(start + offset)
    elif raw[offset] == 0x2D and raw[offset + 1] == 0xE9:  # PUSH.W/STMDB SP!
        seeds.add(start + offset)

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

if len(raw) > 0x10 and raw[:16] == application_marker:
    first = toAddr(start + 0x10)
    symbol_table = currentProgram.getSymbolTable()
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
