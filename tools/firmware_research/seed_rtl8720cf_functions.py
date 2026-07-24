# Seed function discovery for an RTL8720CF/AmebaZ2 Thumb XIP section.
# @category Gree

from jarray import zeros
from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
base = toAddr(args[0] if len(args) else "0x9B000140")
memory = currentProgram.getMemory()
block = memory.getBlock(base)
if block is None or not block.isInitialized():
    raise RuntimeError("no initialized memory block at %s" % base)

raw_java = zeros(int(block.getSize()), "b")
count = memory.getBytes(block.getStart(), raw_java)
raw = bytearray((value & 0xFF) for value in raw_java[:count])
start = block.getStart().getOffset()
end = block.getEnd().getOffset()
seeds = set()
marker = bytearray(ord(value) for value in "AmebaZIIRTL8710C")
if len(raw) > 0x10 and raw[:16] == marker:
    seeds.add(start + 0x10)

for offset in range(0, len(raw) - 3, 2):
    value = (raw[offset] | (raw[offset + 1] << 8) |
             (raw[offset + 2] << 16) | (raw[offset + 3] << 24))
    target = value & ~1
    if (value & 1) and start <= target <= end:
        seeds.add(target)
    if raw[offset + 1] == 0xB5:
        seeds.add(start + offset)
    elif raw[offset] == 0x2D and raw[offset + 1] == 0xE9:
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

if len(raw) > 0x10 and raw[:16] == marker:
    first = toAddr(start + 0x10)
    table = currentProgram.getSymbolTable()
    try:
        table.createLabel(first, "rtl8720cf_application_start", SourceType.USER_DEFINED)
    except Exception:
        pass
    try:
        table.addExternalEntryPoint(first)
    except Exception:
        pass

print("RTL8720CF seeds=%d decoded=%d rejected=%d block=%s-%s" %
      (len(seeds), seeded, failed, block.getStart(), block.getEnd()))
