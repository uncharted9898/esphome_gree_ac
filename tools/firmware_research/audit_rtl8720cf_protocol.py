# Broad Ghidra audit of the GREE application inside RTL8720CF firmware.
# @category Gree
#
# This script is intentionally evidence-oriented. It reports strings, raw
# pointer tables, function hashes, scalar constants, call relationships and
# decompilations without assigning physical telemetry meanings by appearance.

from __future__ import print_function

from collections import deque
import hashlib
from jarray import zeros
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.scalar import Scalar

ARGS = getScriptArgs()
OUT_PATH = ARGS[0] if len(ARGS) > 0 else "/tmp/rtl8720cf-audit.txt"
LABEL = ARGS[1] if len(ARGS) > 1 else currentProgram.getName()

KEYWORDS = [
    "elcen", "energy", "electric", "electricity", "power", "watt", "kwh",
    "current", "amp", "voltage", "volt", "compressor", "comp", "frequency",
    "freq", "hz", "eev", "exv", "valve", "load", "service", "diagnostic",
    "diag", "fault", "error", "status", "report", "selector", "query",
    "property", "attribute", "uart", "serial", "gree", "gatf", "gatr",
    "gatd", "ghex", "hum", "temperature", "thermistor", "outdoor",
    "indoor", "month", "meter", "capacity", "rated", "rpm", "speed",
    "bus", "phase", "module", "protocol", "cloud", "upload", "download",
]

RESPONSE_COMMANDS = set([0x31, 0x32, 0x33, 0x34, 0x35, 0x40, 0x44, 0x45, 0x46, 0x47])
REQUEST_COMMANDS = set([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B])
MAX_DECOMPILED_FUNCTIONS = 500
CALL_GRAPH_DEPTH = 2

out = open(OUT_PATH, "w")


def emit(value=""):
    out.write(str(value) + "\n")


def read_block(block):
    size = int(block.getSize())
    raw = zeros(size, "b")
    read = currentProgram.getMemory().getBytes(block.getStart(), raw)
    if read < 0:
        raise RuntimeError("unable to read memory block %s" % block.getName())
    return bytearray((value & 0xFF) for value in raw[:read])


def sha256_hex(data):
    # Jython's ``str(bytearray)`` is a representation, not the byte payload.
    raw = "".join(chr(value & 0xFF) for value in data)
    return hashlib.sha256(raw).hexdigest()


def function_instruction_bytes(function):
    chunks = []
    iterator = currentProgram.getListing().getInstructions(function.getBody(), True)
    while iterator.hasNext():
        insn = iterator.next()
        try:
            chunks.append(bytearray((value & 0xFF) for value in insn.getBytes()))
        except Exception:
            pass
    merged = bytearray()
    for chunk in chunks:
        merged.extend(chunk)
    return merged


def function_scalars(function):
    values = set()
    iterator = currentProgram.getListing().getInstructions(function.getBody(), True)
    while iterator.hasNext():
        insn = iterator.next()
        for op_index in range(insn.getNumOperands()):
            try:
                objects = insn.getOpObjects(op_index)
            except Exception:
                objects = []
            for obj in objects:
                if isinstance(obj, Scalar):
                    try:
                        values.add(int(obj.getUnsignedValue()))
                    except Exception:
                        pass
    return values


def function_at_pointer(value):
    if value == 0 or value == 0xFFFFFFFF:
        return None
    address_value = value & ~1
    try:
        address = toAddr(address_value)
    except Exception:
        return None
    function = currentProgram.getFunctionManager().getFunctionAt(address)
    if function is None:
        function = currentProgram.getFunctionManager().getFunctionContaining(address)
    return function


def ascii_strings(block, data, minimum=4):
    results = []
    start = None
    for index in range(len(data)):
        value = data[index]
        printable = 0x20 <= value <= 0x7E
        if printable and start is None:
            start = index
        if printable:
            continue
        if start is not None and index - start >= minimum:
            text = data[start:index].decode("ascii", "replace")
            results.append((block.getStart().add(start), text))
        start = None
    if start is not None and len(data) - start >= minimum:
        results.append((block.getStart().add(start), data[start:].decode("ascii", "replace")))
    return results


def dword_context(block, data, offset, radius=8):
    aligned = offset & ~3
    start = max(0, aligned - radius * 4)
    end = min(len(data), aligned + (radius + 1) * 4)
    rows = []
    index = start
    while index + 4 <= end:
        value = data[index] | (data[index + 1] << 8) | (data[index + 2] << 16) | (data[index + 3] << 24)
        rows.append((block.getStart().add(index), value))
        index += 4
    return rows


def references_to(address):
    refs = []
    iterator = currentProgram.getReferenceManager().getReferencesTo(address)
    while iterator.hasNext():
        refs.append(iterator.next())
    return refs


def select_function(function, reason, selected):
    if function is None:
        return
    key = function.getEntryPoint().getOffset()
    if key not in selected:
        selected[key] = {"function": function, "reasons": []}
    if reason not in selected[key]["reasons"]:
        selected[key]["reasons"].append(reason)


memory = currentProgram.getMemory()
listing = currentProgram.getListing()
function_manager = currentProgram.getFunctionManager()

blocks = []
for block in memory.getBlocks():
    if block.isInitialized():
        blocks.append((block, read_block(block)))

emit("GREE RTL8720CF firmware audit")
emit("Label: %s" % LABEL)
emit("Program: %s" % currentProgram.getName())
emit("Image base: %s" % currentProgram.getImageBase())
emit("Processor: %s" % currentProgram.getLanguage().getLanguageID())
emit("Compiler: %s" % currentProgram.getCompilerSpec().getCompilerSpecID())
emit("Memory blocks:")
for block, data in blocks:
    emit("  %s %s-%s size=%d sha256=%s" % (
        block.getName(), block.getStart(), block.getEnd(), len(data), sha256_hex(data)
    ))
emit()

all_strings = []
for block, data in blocks:
    all_strings.extend(ascii_strings(block, data))
keyword_strings = []
for address, text in all_strings:
    lower = text.lower()
    matches = sorted(set(keyword for keyword in KEYWORDS if keyword in lower))
    if matches:
        keyword_strings.append((address, text, matches))

emit("=== KEYWORD STRINGS ===")
for address, text, matches in keyword_strings:
    emit("%s keywords=%s text=%s" % (address, ",".join(matches), text.replace("\n", "\\n")))
emit()

selected = {}
pointer_records = []

for string_address, text, matches in keyword_strings:
    # Normal Ghidra references to the string address.
    for ref in references_to(string_address):
        source = ref.getFromAddress()
        function = function_manager.getFunctionContaining(source)
        select_function(function, "string-ref %s -> %s %s" % (source, string_address, text), selected)

    # Raw little-endian pointers catch property tables that auto-analysis did
    # not classify as references.
    value = string_address.getOffset() & 0xFFFFFFFF
    needle = bytearray([
        value & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 24) & 0xFF,
    ])
    for block, data in blocks:
        cursor = 0
        while True:
            offset = data.find(needle, cursor)
            if offset < 0:
                break
            pointer_address = block.getStart().add(offset)
            context = dword_context(block, data, offset)
            pointer_records.append((string_address, text, pointer_address, context))
            for ref in references_to(pointer_address):
                source = ref.getFromAddress()
                select_function(
                    function_manager.getFunctionContaining(source),
                    "table-ref %s -> %s containing %s" % (source, pointer_address, text),
                    selected,
                )
            for context_address, context_value in context:
                function = function_at_pointer(context_value)
                if function is not None:
                    select_function(
                        function,
                        "function-pointer near %s for property %s" % (pointer_address, text),
                        selected,
                    )
            cursor = offset + 1

emit("=== RAW PROPERTY/TABLE POINTERS ===")
for string_address, text, pointer_address, context in pointer_records:
    emit("string %s %s pointer-at %s" % (string_address, text, pointer_address))
    emit("  " + " ".join("%s=%08X" % (address, value) for address, value in context))
emit()

function_metadata = {}
function_iterator = function_manager.getFunctions(True)
while function_iterator.hasNext():
    function = function_iterator.next()
    scalars = function_scalars(function)
    instruction_bytes = function_instruction_bytes(function)
    entry = function.getEntryPoint().getOffset()
    response_hits = sorted(RESPONSE_COMMANDS.intersection(scalars))
    request_hits = sorted(REQUEST_COMMANDS.intersection(scalars))
    metadata = {
        "function": function,
        "hash": sha256_hex(instruction_bytes),
        "size": len(instruction_bytes),
        "scalars": scalars,
        "response_hits": response_hits,
        "request_hits": request_hits,
    }
    function_metadata[entry] = metadata

    # Broad candidates for frame constructors and receive dispatchers.
    if len(response_hits) >= 4:
        select_function(function, "response-command-dispatch constants=%s" % response_hits, selected)
    if 0x7E in scalars and (request_hits or response_hits) and (0x19 in scalars or 0x2C in scalars or 0x2F in scalars):
        select_function(
            function,
            "frame-builder constants req=%s resp=%s" % (request_hits, response_hits),
            selected,
        )
    if 0x7E in scalars and 0x03 in scalars and 0x19 in scalars:
        select_function(function, "long-command-0x03 builder signature", selected)

# Expand the call graph. Wrappers, checksums, UART writes, and response handlers
# frequently contain no strings themselves.
queue = deque()
for key in sorted(selected.keys()):
    queue.append((selected[key]["function"], 0))
visited_depth = {}
while queue:
    function, depth = queue.popleft()
    key = function.getEntryPoint().getOffset()
    if key in visited_depth and visited_depth[key] <= depth:
        continue
    visited_depth[key] = depth
    if depth >= CALL_GRAPH_DEPTH:
        continue
    try:
        callees = function.getCalledFunctions(monitor)
        for callee in callees:
            select_function(callee, "call-graph callee of %s" % function.getEntryPoint(), selected)
            queue.append((callee, depth + 1))
    except Exception:
        pass
    try:
        callers = function.getCallingFunctions(monitor)
        for caller in callers:
            select_function(caller, "call-graph caller of %s" % function.getEntryPoint(), selected)
            queue.append((caller, depth + 1))
    except Exception:
        pass

emit("=== FUNCTION INDEX ===")
for key in sorted(selected.keys()):
    function = selected[key]["function"]
    metadata = function_metadata.get(key)
    if metadata is None:
        instruction_bytes = function_instruction_bytes(function)
        metadata = {
            "hash": sha256_hex(instruction_bytes),
            "size": len(instruction_bytes),
            "scalars": function_scalars(function),
            "response_hits": [],
            "request_hits": [],
        }
    callers = []
    callees = []
    try:
        callers = sorted(fn.getEntryPoint().toString() for fn in function.getCallingFunctions(monitor))
    except Exception:
        pass
    try:
        callees = sorted(fn.getEntryPoint().toString() for fn in function.getCalledFunctions(monitor))
    except Exception:
        pass
    emit("%s name=%s size=%d hash=%s" % (
        function.getEntryPoint(), function.getName(), metadata["size"], metadata["hash"]
    ))
    emit("  reasons: %s" % " || ".join(selected[key]["reasons"]))
    emit("  response-command-constants: %s" % metadata.get("response_hits", []))
    emit("  request-command-constants: %s" % metadata.get("request_hits", []))
    emit("  scalars: %s" % " ".join("0x%X" % value for value in sorted(metadata["scalars"]) if value <= 0xFFFFFFFF))
    emit("  callers: %s" % " ".join(callers))
    emit("  callees: %s" % " ".join(callees))
emit()

# Scan initialized blocks for embedded checksum-valid frames.
emit("=== CHECKSUM-VALID 7E 7E FRAMES ===")
for block, data in blocks:
    index = 0
    while index + 5 <= len(data):
        if data[index:index + 2] != b"\x7e\x7e":
            index += 1
            continue
        declared = data[index + 2]
        total = declared + 3
        if total < 5 or total > 200 or index + total > len(data):
            index += 1
            continue
        raw = data[index:index + total]
        if (sum(bytearray(raw[2:-1])) & 0xFF) != raw[-1]:
            index += 1
            continue
        emit("%s len=%d cmd=0x%02X %s" % (
            block.getStart().add(index), total, raw[3], " ".join("%02X" % b for b in bytearray(raw))
        ))
        index += total
emit()

# Decompile last, so all structural output remains available even if an
# individual function decompiler fails.
decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
emit("=== DECOMPILED FUNCTIONS ===")
count = 0
for key in sorted(selected.keys()):
    if count >= MAX_DECOMPILED_FUNCTIONS:
        emit("DECOMPILATION LIMIT REACHED: %d" % MAX_DECOMPILED_FUNCTIONS)
        break
    function = selected[key]["function"]
    emit("\n----- %s %s -----" % (function.getEntryPoint(), function.getName()))
    emit("reasons: %s" % " || ".join(selected[key]["reasons"]))
    try:
        result = decompiler.decompileFunction(function, 120, monitor)
        if result.decompileCompleted():
            emit(result.getDecompiledFunction().getC())
        else:
            emit("DECOMPILE FAILED: %s" % result.getErrorMessage())
    except Exception as exc:
        emit("DECOMPILE EXCEPTION: %s" % exc)
    count += 1

out.close()
print("Wrote RTL8720CF audit to %s" % OUT_PATH)
