# Ghidra headless post-script for Gree RTL8720CF/AmebaZ2 XIP applications.
# @category Gree
#
# Import the first XIP application section at its EntryHeader section_base
# (0x9B000140 for the archived v1.21 and v1.53 images), run auto-analysis, and
# invoke this script with an output path.  The later 0x9B800140 XIP section is
# Realtek radio firmware and should be analyzed separately, not merged into the
# Gree application report.

from ghidra.app.decompiler import DecompInterface

KEYWORDS = [
    "uart", "serial", "energy", "electric", "power", "watt", "kwh",
    "current", "amp", "voltage", "volt", "compressor", "comp",
    "frequency", "freq", "hz", "eev", "exv", "valve", "load",
    "service", "diagnostic", "diag", "fault", "error", "status",
    "report", "selector", "query", "gree", "gatf", "gatr", "gatd",
    "ghex",
]

OUT_PATH = getScriptArgs()[0] if len(getScriptArgs()) else "/tmp/rtl8720cf-protocol.txt"
out = open(OUT_PATH, "w")


def emit(value=""):
    out.write(str(value) + "\n")


listing = currentProgram.getListing()
refman = currentProgram.getReferenceManager()
fm = currentProgram.getFunctionManager()
mem = currentProgram.getMemory()

emit("Gree RTL8720CF protocol string/xref/decompiler report")
emit("Program: %s" % currentProgram.getName())
emit("Image base: %s" % currentProgram.getImageBase())
emit("Memory blocks:")
for block in mem.getBlocks():
    emit("  %s %s-%s" % (block.getName(), block.getStart(), block.getEnd()))
emit()

matched = []
data_it = listing.getDefinedData(True)
while data_it.hasNext():
    data = data_it.next()
    try:
        value = data.getValue()
        text = str(value) if value is not None else ""
    except:
        text = ""
    lower = text.lower()
    if text and any(keyword in lower for keyword in KEYWORDS):
        matched.append((data.getAddress(), text))

emit("=== MATCHED STRINGS ===")
for address, text in matched:
    emit("%s  %s" % (address, text.replace("\n", "\\n")))
emit()

selected = {}
for address, text in matched:
    refs = refman.getReferencesTo(address)
    while refs.hasNext():
        ref = refs.next()
        source = ref.getFromAddress()
        function = fm.getFunctionContaining(source)
        if function is None:
            emit("unowned ref %s -> %s %s" % (source, address, text))
            continue
        key = function.getEntryPoint().toString()
        selected.setdefault(key, {"fn": function, "reasons": []})["reasons"].append(
            (source, address, text)
        )

# Include one call-graph hop in each direction.  UART wrappers often contain no
# useful string themselves but are adjacent to string-bearing dispatch code.
for key in list(selected.keys()):
    function = selected[key]["fn"]
    for called in function.getCalledFunctions(monitor):
        called_key = called.getEntryPoint().toString()
        selected.setdefault(called_key, {"fn": called, "reasons": []})
    for caller in function.getCallingFunctions(monitor):
        caller_key = caller.getEntryPoint().toString()
        selected.setdefault(caller_key, {"fn": caller, "reasons": []})

emit("=== SELECTED FUNCTIONS ===")
for key in sorted(selected.keys()):
    function = selected[key]["fn"]
    emit("%s %s" % (function.getEntryPoint(), function.getName()))
    for source, address, text in selected[key]["reasons"]:
        emit("  ref %s -> %s %s" % (source, address, text.replace("\n", "\\n")))
emit()

decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
emit("=== DECOMPILED FUNCTIONS ===")
for key in sorted(selected.keys()):
    function = selected[key]["fn"]
    emit("\n----- %s %s -----" % (function.getEntryPoint(), function.getName()))
    try:
        result = decompiler.decompileFunction(function, 90, monitor)
        if result.decompileCompleted():
            emit(result.getDecompiledFunction().getC())
        else:
            emit("DECOMPILE FAILED: %s" % result.getErrorMessage())
    except Exception as exc:
        emit("DECOMPILE EXCEPTION: %s" % exc)

emit("\n=== CHECKSUM-VALID 7E 7E FRAMES ===")
for block in mem.getBlocks():
    if not block.isInitialized():
        continue
    address = block.getStart()
    end = block.getEnd()
    while address.compareTo(end) <= 0:
        try:
            if address.add(4).compareTo(end) > 0:
                break
            if (mem.getByte(address) & 0xFF) != 0x7E or (mem.getByte(address.add(1)) & 0xFF) != 0x7E:
                address = address.add(1)
                continue
            declared = mem.getByte(address.add(2)) & 0xFF
            total = declared + 3
            if total < 5 or total > 200 or address.add(total - 1).compareTo(end) > 0:
                address = address.add(1)
                continue
            raw = []
            checksum = 0
            for index in range(total):
                byte = mem.getByte(address.add(index)) & 0xFF
                raw.append(byte)
                if 2 <= index < total - 1:
                    checksum = (checksum + byte) & 0xFF
            if checksum == raw[-1]:
                emit("%s len=%d cmd=0x%02X %s" % (
                    address,
                    total,
                    raw[3],
                    " ".join("%02X" % byte for byte in raw),
                ))
                address = address.add(total)
                continue
        except Exception as exc:
            emit("scan exception at %s: %s" % (address, exc))
        address = address.add(1)

out.close()
print("Wrote RTL8720CF protocol report to %s" % OUT_PATH)
