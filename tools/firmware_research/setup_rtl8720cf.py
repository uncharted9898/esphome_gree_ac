# Prepare a raw RTL8720CF/AmebaZ2 XIP section for Ghidra analysis.
# @category Gree

from java.math import BigInteger
from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
if len(args) < 1:
    raise RuntimeError("usage: setup_rtl8720cf.py <base-address> [entry-address|-]")

base = toAddr(args[0])
entry_arg = args[1] if len(args) > 1 else "-"
block = currentProgram.getMemory().getBlock(base)
if block is None or not block.isInitialized():
    raise RuntimeError("no initialized memory block at %s" % base)

program_context = currentProgram.getProgramContext()
tmode = currentProgram.getLanguage().getRegister("TMode")
if tmode is not None:
    program_context.setValue(tmode, block.getStart(), block.getEnd(), BigInteger.ONE)

if entry_arg != "-":
    entry = toAddr(int(entry_arg, 0) & ~1)
    symbol_table = currentProgram.getSymbolTable()
    try:
        symbol_table.createLabel(entry, "rtl8720cf_seed", SourceType.USER_DEFINED)
    except Exception:
        pass
    try:
        symbol_table.addExternalEntryPoint(entry)
    except Exception:
        pass
    disassemble(entry)
    if getInstructionAt(entry) is None:
        raise RuntimeError("entry point did not decode as Thumb code: %s" % entry)
    if getFunctionAt(entry) is None:
        createFunction(entry, "rtl8720cf_seed")

print("Configured Thumb mode for %s-%s" % (block.getStart(), block.getEnd()))
