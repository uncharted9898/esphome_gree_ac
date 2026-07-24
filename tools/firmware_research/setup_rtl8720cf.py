# Prepare a raw RTL8720CF/AmebaZ2 XIP application for Ghidra analysis.
# @category Gree

from java.math import BigInteger
from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
if len(args) < 1:
    raise RuntimeError("usage: setup_rtl8720cf.py <base-address> [entry-address]")

base = toAddr(args[0])
entry_value = int(args[1], 0) if len(args) > 1 else base.getOffset()
entry = toAddr(entry_value & ~1)

program_context = currentProgram.getProgramContext()
tmode = currentProgram.getLanguage().getRegister("TMode")
if tmode is not None:
    program_context.setValue(tmode, base, currentProgram.getMaxAddress(), BigInteger.ONE)

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
if getFunctionAt(entry) is None:
    createFunction(entry, "rtl8720cf_seed")

print("Configured Thumb mode from %s and seed point %s" % (base, entry))
