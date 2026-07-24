# Evidence-oriented Ghidra audit for one raw GREE RTL8720CF XIP section.
# @category Gree

from __future__ import print_function

from collections import deque
import hashlib
from jarray import zeros
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.scalar import Scalar

args = getScriptArgs()
OUT_PATH = args[0] if len(args) else "/tmp/rtl8720cf-audit.txt"
LABEL = args[1] if len(args) > 1 else currentProgram.getName()

KEYWORDS = [
    "elcen", "elc", "energy", "electric", "electricity", "power", "watt",
    "kwh", "current", "amp", "voltage", "volt", "compressor", "comp",
    "compressorfqy", "compressortem", "frequency", "freq", "fqy", "hz",
    "eev", "exv", "valve", "load", "gear", "flow", "service",
    "diagnostic", "diag", "fault", "error", "status", "report", "selector",
    "query", "property", "attribute", "uart", "serial", "gree", "gatf",
    "gatr", "gatd", "ghex", "hum", "temperature", "thermistor", "outdoor",
    "indoor", "month", "meter", "capacity", "rated", "rpm", "speed", "bus",
    "phase", "module", "protocol", "cloud", "upload", "download", "inboard",
    "outboard", "midtype", "devinfo", "energyflow", "watttmp", "fantmod",
]
RESPONSE_COMMANDS = set([0x31, 0x32, 0x33, 0x34, 0x35, 0x40, 0x44, 0x45,
                         0x46, 0x47, 0x4D, 0x52])
REQUEST_COMMANDS = set([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0x0B])
MAX_DECOMPILED_FUNCTIONS = 1000
CALL_GRAPH_DEPTH = 3

out = open(OUT_PATH, "w")


def emit(value=""):
    out.write(str(value) + "\n")


def read_block(block):
    size = int(block.getSize())
    raw_java = zeros(size, "b")
    count = currentProgram.getMemory().getBytes(block.getStart(), raw_java)
    if count < 0:
        raise RuntimeError("unable to read block %s" % block.getName())
    return bytearray((value & 0xFF) for value in raw_java[:count])


def binary_string(data):
    return "".join(chr(value & 0xFF) for value in data)


def digest(data):
    return hashlib.sha256(binary_string(data)).hexdigest()


def function_bytes(function):
    merged = bytearray()
    iterator = currentProgram.getListing().getInstructions(function.getBody(), True)
    while iterator.hasNext():
        instruction = iterator.next()
        try:
            merged.extend(bytearray((value & 0xFF) for value in instruction.getBytes()))
        except Exception:
            pass
    return merged


def function_scalars(function):
    values = set()
    iterator = currentProgram.getListing().getInstructions(function.getBody(), True)
    while iterator.hasNext():
        instruction = iterator.next()
        for operand in range(instruction.getNumOperands()):
            try:
                objects = instruction.getOpObjects(operand)
            except Exception:
                objects = []
            for obj in objects:
                if isinstance(obj, Scalar):
                    try:
                        values.add(int(obj.getUnsignedValue()))
                    except Exception:
                        pass
    return values


def ascii_strings(block, data, minimum=4):
    result = []
    begin = None
    for index in range(len(data)):
        value = data[index]
        if 0x20 <= value <= 0x7E:
            if begin is None:
                begin = index
            continue
        if begin is not None and index - begin >= minimum:
            result.append((block.getStart().add(begin),
                           data[begin:index].decode("ascii", "replace")))
        begin = None
    if begin is not None and len(data) - begin >= minimum:
        result.append((block.getStart().add(begin),
                       data[begin:].decode("ascii", "replace")))
    return result


def refs_to(address):
    result = []
    iterator = currentProgram.getReferenceManager().getReferencesTo(address)
    while iterator.hasNext():
        result.append(iterator.next())
    return result


def select(function, reason, selected):
    if function is None:
        return
    key = function.getEntryPoint().getOffset()
    if key not in selected:
        selected[key] = {"function": function, "reasons": []}
    if reason not in selected[key]["reasons"]:
        selected[key]["reasons"].append(reason)


def pointer_function(value):
    if value in (0, 0xFFFFFFFF):
        return None
    try:
        address = toAddr(value & ~1)
    except Exception:
        return None
    function = currentProgram.getFunctionManager().getFunctionAt(address)
    if function is None:
        function = currentProgram.getFunctionManager().getFunctionContaining(address)
    return function


def dword_context(block, data, offset, radius=12):
    aligned = offset & ~3
    start = max(0, aligned - radius * 4)
    end = min(len(data), aligned + (radius + 1) * 4)
    result = []
    for index in range(start, end - 3, 4):
        value = (data[index] | (data[index + 1] << 8) |
                 (data[index + 2] << 16) | (data[index + 3] << 24))
        result.append((block.getStart().add(index), value))
    return result


def instruction_text(function):
    lines = []
    iterator = currentProgram.getListing().getInstructions(function.getBody(), True)
    while iterator.hasNext():
        instruction = iterator.next()
        try:
            raw = " ".join("%02X" % (value & 0xFF)
                           for value in instruction.getBytes())
        except Exception:
            raw = ""
        lines.append("%s %-11s %s" %
                     (instruction.getAddress(), raw, instruction.toString()))
    return lines


memory = currentProgram.getMemory()
functions = currentProgram.getFunctionManager()
blocks = []
for block in memory.getBlocks():
    if block.isInitialized():
        blocks.append((block, read_block(block)))

emit("GREE RTL8720CF firmware protocol audit")
emit("Label: %s" % LABEL)
emit("Program: %s" % currentProgram.getName())
emit("Image base: %s" % currentProgram.getImageBase())
emit("Language: %s" % currentProgram.getLanguage().getLanguageID())
emit("Compiler: %s" % currentProgram.getCompilerSpec().getCompilerSpecID())
emit("Memory blocks:")
for block, data in blocks:
    emit("  %s %s-%s bytes=%d sha256=%s" %
         (block.getName(), block.getStart(), block.getEnd(), len(data),
          digest(data)))
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
    emit("%s keywords=%s text=%s" %
         (address, ",".join(matches), text.replace("\n", "\\n")))
emit()

selected = {}
pointer_records = []
for string_address, text, matches in keyword_strings:
    for ref in refs_to(string_address):
        source = ref.getFromAddress()
        select(functions.getFunctionContaining(source),
               "string-ref %s -> %s %s" %
               (source, string_address, text), selected)

    value = string_address.getOffset() & 0xFFFFFFFF
    needle = bytearray([value & 0xFF, (value >> 8) & 0xFF,
                        (value >> 16) & 0xFF, (value >> 24) & 0xFF])
    for block, data in blocks:
        cursor = 0
        while True:
            offset = data.find(needle, cursor)
            if offset < 0:
                break
            pointer_address = block.getStart().add(offset)
            context = dword_context(block, data, offset)
            pointer_records.append((string_address, text, pointer_address, context))
            for ref in refs_to(pointer_address):
                source = ref.getFromAddress()
                select(functions.getFunctionContaining(source),
                       "table-ref %s -> %s containing %s" %
                       (source, pointer_address, text), selected)
            for context_address, context_value in context:
                select(pointer_function(context_value),
                       "function-pointer near %s property=%s" %
                       (pointer_address, text), selected)
            cursor = offset + 1

emit("=== RAW PROPERTY/TABLE POINTERS ===")
for string_address, text, pointer_address, context in pointer_records:
    emit("string %s %s pointer-at %s" %
         (string_address, text, pointer_address))
    emit("  " + " ".join("%s=%08X" % pair for pair in context))
emit()

metadata = {}
iterator = functions.getFunctions(True)
while iterator.hasNext():
    function = iterator.next()
    raw = function_bytes(function)
    scalars = function_scalars(function)
    entry = function.getEntryPoint().getOffset()
    response_hits = sorted(RESPONSE_COMMANDS.intersection(scalars))
    request_hits = sorted(REQUEST_COMMANDS.intersection(scalars))
    metadata[entry] = {
        "function": function,
        "size": len(raw),
        "hash": digest(raw),
        "scalars": scalars,
        "response_hits": response_hits,
        "request_hits": request_hits,
    }

    if len(response_hits) >= 3:
        select(function, "multi-response dispatcher=%s" % response_hits,
               selected)
    if 0x7E in scalars and request_hits:
        select(function, "sync/request constants=%s" % request_hits, selected)
    if 0x7E in scalars and (0x19 in scalars or 0x2C in scalars or
                            0x2F in scalars):
        select(function, "frame length and sync constants", selected)
    if 0x19 in scalars and 0x03 in scalars:
        select(function, "long command-0x03 candidate", selected)
    if 0x40 in scalars and (0x35 in scalars or 0x19 in scalars or
                            0x03 in scalars):
        select(function, "0x40 electrical-report candidate", selected)

emit("=== ALL FUNCTION INDEX ===")
for entry in sorted(metadata.keys()):
    item = metadata[entry]
    small = sorted(value for value in item["scalars"] if value <= 0xFF)
    emit("%08X name=%s size=%d hash=%s responses=%s requests=%s bytes=%s" %
         (entry, item["function"].getName(), item["size"], item["hash"],
          item["response_hits"], item["request_hits"],
          " ".join("%02X" % value for value in small)))
emit()

queue = deque((selected[key]["function"], 0) for key in sorted(selected.keys()))
visited = {}
while queue:
    function, depth = queue.popleft()
    key = function.getEntryPoint().getOffset()
    if key in visited and visited[key] <= depth:
        continue
    visited[key] = depth
    if depth >= CALL_GRAPH_DEPTH:
        continue
    try:
        for callee in function.getCalledFunctions(monitor):
            select(callee, "callee of %s" % function.getEntryPoint(), selected)
            queue.append((callee, depth + 1))
    except Exception:
        pass
    try:
        for caller in function.getCallingFunctions(monitor):
            select(caller, "caller of %s" % function.getEntryPoint(), selected)
            queue.append((caller, depth + 1))
    except Exception:
        pass

emit("=== SELECTED FUNCTION INDEX ===")
for key in sorted(selected.keys()):
    function = selected[key]["function"]
    item = metadata.get(key)
    if item is None:
        raw = function_bytes(function)
        item = {"size": len(raw), "hash": digest(raw),
                "scalars": function_scalars(function),
                "response_hits": [], "request_hits": []}
    callers = []
    callees = []
    try:
        callers = sorted(caller.getEntryPoint().toString()
                         for caller in function.getCallingFunctions(monitor))
    except Exception:
        pass
    try:
        callees = sorted(callee.getEntryPoint().toString()
                         for callee in function.getCalledFunctions(monitor))
    except Exception:
        pass
    emit("%s name=%s size=%d hash=%s" %
         (function.getEntryPoint(), function.getName(), item["size"],
          item["hash"]))
    emit("  reasons: %s" % " || ".join(selected[key]["reasons"]))
    emit("  responses: %s requests: %s" %
         (item.get("response_hits", []), item.get("request_hits", [])))
    emit("  scalars: %s" % " ".join("0x%X" % value for value in
                                     sorted(item["scalars"])
                                     if value <= 0xFFFFFFFF))
    emit("  callers: %s" % " ".join(callers))
    emit("  callees: %s" % " ".join(callees))
emit()

emit("=== SELECTED FUNCTION DISASSEMBLY ===")
for key in sorted(selected.keys()):
    function = selected[key]["function"]
    emit("\n----- %s %s -----" %
         (function.getEntryPoint(), function.getName()))
    emit("reasons: %s" % " || ".join(selected[key]["reasons"]))
    for line in instruction_text(function):
        emit(line)
emit()

emit("=== CHECKSUM-VALID 7E 7E FRAMES ===")
for block, data in blocks:
    index = 0
    while index + 5 <= len(data):
        if data[index:index + 2] != bytearray([0x7E, 0x7E]):
            index += 1
            continue
        declared = data[index + 2]
        total = declared + 3
        if total < 5 or total > 200 or index + total > len(data):
            index += 1
            continue
        raw = data[index:index + total]
        if (sum(raw[2:-1]) & 0xFF) != raw[-1]:
            index += 1
            continue
        emit("%s len=%d cmd=0x%02X %s" %
             (block.getStart().add(index), total, raw[3],
              " ".join("%02X" % value for value in raw)))
        index += total
emit()

decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
emit("=== DECOMPILED FUNCTIONS ===")
for count, key in enumerate(sorted(selected.keys())):
    if count >= MAX_DECOMPILED_FUNCTIONS:
        emit("DECOMPILATION LIMIT REACHED: %d" % MAX_DECOMPILED_FUNCTIONS)
        break
    function = selected[key]["function"]
    emit("\n----- %s %s -----" %
         (function.getEntryPoint(), function.getName()))
    emit("reasons: %s" % " || ".join(selected[key]["reasons"]))
    try:
        result = decompiler.decompileFunction(function, 120, monitor)
        if result.decompileCompleted():
            emit(result.getDecompiledFunction().getC())
        else:
            emit("DECOMPILE FAILED: %s" % result.getErrorMessage())
    except Exception as exc:
        emit("DECOMPILE EXCEPTION: %s" % exc)

out.close()
print("Wrote RTL8720CF audit to %s" % OUT_PATH)
