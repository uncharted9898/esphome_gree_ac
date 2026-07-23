# Ghidra headless post-script for the CS532AX/MT7687 firmware.
# @category Gree

from ghidra.app.decompiler import DecompInterface

KEYWORDS = [
    "fault", "energy", "timer", "outdoor", "inner", "outer", "time", "sync",
    "mac", "uart", "report", "status", "device", "info", "heartbeat", "ctrl",
    "control", "temperature", "humidity", "defrost", "elc", "kwh", "devinfo",
]
APP_STRING_START = 0x100D1800
APP_STRING_END = 0x100D6000
# Functions seen constructing, sending, parsing, or validating the appliance
# UART frames. Include these even when they do not reference a diagnostic string.
HELPER_ADDRESSES = [
    0x1007D134, 0x100824B8, 0x100824F2,
    0x1008271C, 0x1008273A, 0x1008274A, 0x1008275C, 0x10082778,
    0x100827BE, 0x100827C6, 0x100827E2, 0x100827E6, 0x100827EA,
    0x100827EE, 0x100827F2, 0x100827F6, 0x100827FA, 0x100827FE,
    0x10082802, 0x10082806, 0x1008280A, 0x10082812, 0x1008282E,
    0x10082834, 0x10082844, 0x10082858, 0x10082884, 0x1008289C,
    0x100828B0, 0x100828CE, 0x100828DC, 0x100828E2,
    0x1008291E, 0x100829AC,
    0x100839D6, 0x100839DC, 0x100839E2, 0x100839E8,
    0x10083A12, 0x10083A18, 0x10083A5A, 0x10083A60, 0x10083A66,
    0x10083A96, 0x10083A9C, 0x10083B02,
    0x100840D8, 0x1008419C, 0x10084310, 0x1008434A, 0x10084394,
]

OUT_PATH = getScriptArgs()[0] if len(getScriptArgs()) else "/tmp/gree-ghidra-report.txt"
out = open(OUT_PATH, "w")

def emit(s=""):
    out.write(str(s) + "\n")

emit("Gree CS532AX MT7687 firmware string/xref/decompiler report")
emit("Program: %s" % currentProgram.getName())
emit("Image base property: %s" % currentProgram.getImageBase())

listing = currentProgram.getListing()
refman = currentProgram.getReferenceManager()
fm = currentProgram.getFunctionManager()
mem = currentProgram.getMemory()
space = currentProgram.getAddressFactory().getDefaultAddressSpace()

emit("Memory blocks:")
for block in mem.getBlocks():
    emit("  %s %s-%s" % (block.getName(), block.getStart(), block.getEnd()))
emit()

strings = []
data_it = listing.getDefinedData(True)
while data_it.hasNext():
    d = data_it.next()
    numeric = d.getAddress().getOffset()
    if numeric < APP_STRING_START or numeric >= APP_STRING_END:
        continue
    try:
        value = d.getValue()
        text = str(value) if value is not None else ""
    except:
        text = ""
    low = text.lower()
    if text and any(k in low for k in KEYWORDS):
        strings.append((d.getAddress(), text))

emit("=== MATCHED APP STRINGS ===")
for addr, text in strings:
    emit("%s  %s" % (addr, text.replace("\n", "\\n")))
emit()

funcs = {}
unowned_refs = []
for addr, text in strings:
    refs = refman.getReferencesTo(addr)
    while refs.hasNext():
        ref = refs.next()
        src = ref.getFromAddress()
        fn = fm.getFunctionContaining(src)
        if fn is not None:
            key = fn.getEntryPoint().toString()
            funcs.setdefault(key, {"fn": fn, "refs": []})["refs"].append((src, addr, text))
        else:
            unowned_refs.append((src, addr, text))

for numeric in HELPER_ADDRESSES:
    addr = space.getAddress(numeric)
    fn = fm.getFunctionAt(addr)
    if fn is None:
        fn = fm.getFunctionContaining(addr)
    if fn is not None:
        key = fn.getEntryPoint().toString()
        funcs.setdefault(key, {"fn": fn, "refs": []})

emit("=== FUNCTIONS SELECTED FOR DECOMPILATION ===")
for key in sorted(funcs.keys()):
    fn = funcs[key]["fn"]
    emit("%s  %s" % (fn.getEntryPoint(), fn.getName()))
    for src, saddr, text in funcs[key]["refs"]:
        emit("  ref %s -> %s  %s" % (src, saddr, text.replace("\n", "\\n")))
emit()

emit("=== STRING REFERENCES OUTSIDE DEFINED FUNCTIONS ===")
for src, saddr, text in unowned_refs:
    emit("%s -> %s  %s" % (src, saddr, text.replace("\n", "\\n")))
emit()

decomp = DecompInterface()
decomp.openProgram(currentProgram)
emit("=== DECOMPILED FUNCTIONS ===")
for key in sorted(funcs.keys()):
    fn = funcs[key]["fn"]
    emit("\n----- %s %s -----" % (fn.getEntryPoint(), fn.getName()))
    try:
        result = decomp.decompileFunction(fn, 60, monitor)
        if result.decompileCompleted():
            emit(result.getDecompiledFunction().getC())
        else:
            emit("DECOMPILE FAILED: %s" % result.getErrorMessage())
    except Exception as exc:
        emit("DECOMPILE EXCEPTION: %s" % exc)

emit("\n=== 7E 7E FRAME-LIKE BYTE SEQUENCES ===")
for block in mem.getBlocks():
    if not block.isInitialized():
        continue
    start = block.getStart()
    end = block.getEnd()
    addr = start
    while addr.compareTo(end) <= 0:
        try:
            if addr.add(2).compareTo(end) <= 0 and (mem.getByte(addr) & 0xff) == 0x7e and (mem.getByte(addr.add(1)) & 0xff) == 0x7e:
                length = mem.getByte(addr.add(2)) & 0xff
                total = length + 3
                if 5 <= total <= 200 and addr.add(total - 1).compareTo(end) <= 0:
                    raw = []
                    checksum = 0
                    for i in range(total):
                        b = mem.getByte(addr.add(i)) & 0xff
                        raw.append(b)
                        if 2 <= i < total - 1:
                            checksum = (checksum + b) & 0xff
                    valid = checksum == raw[-1]
                    emit("%s len=%d cmd=0x%02X checksum=%s %s" % (
                        addr, total, raw[3], "OK" if valid else "BAD",
                        " ".join("%02X" % b for b in raw)))
                    addr = addr.add(max(1, total))
                    continue
        except Exception as exc:
            emit("scan exception at %s: %s" % (addr, exc))
        addr = addr.add(1)

out.close()
print("Wrote protocol report to %s" % OUT_PATH)
