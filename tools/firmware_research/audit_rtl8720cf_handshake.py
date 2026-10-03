# Evidence-oriented targeted decompilation for the GREE RTL8720CF appliance-UART
# startup handshake. This script starts from the real uart_task receive
# dispatcher instead of selecting functions merely because they contain command
# constants.
# @category Gree

from __future__ import print_function

from collections import deque
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.scalar import Scalar

args = getScriptArgs()
if len(args) < 3:
    raise RuntimeError(
        "usage: audit_rtl8720cf_handshake.py <output> <label> <root[,root...]> [depth]"
    )

OUT_PATH = args[0]
LABEL = args[1]
ROOT_VALUES = [int(value, 0) for value in args[2].split(",") if value]
DEPTH = int(args[3], 0) if len(args) > 3 else 2

listing = currentProgram.getListing()
function_manager = currentProgram.getFunctionManager()

out = open(OUT_PATH, "w")


def emit(value=""):
    out.write(str(value) + "\n")


def function_at(value):
    address = toAddr(value & ~1)
    function = function_manager.getFunctionAt(address)
    if function is None:
        function = function_manager.getFunctionContaining(address)
    return function


def function_scalars(function):
    values = set()
    iterator = listing.getInstructions(function.getBody(), True)
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


selected = {}
queue = deque()
for value in ROOT_VALUES:
    function = function_at(value)
    if function is None:
        emit("MISSING ROOT 0x%08X" % value)
        continue
    key = function.getEntryPoint().getOffset()
    selected[key] = {
        "function": function,
        "depth": 0,
        "reasons": ["root 0x%08X" % value],
    }
    queue.append((function, 0))

while queue:
    function, depth = queue.popleft()
    if depth >= DEPTH:
        continue
    neighbors = []
    try:
        neighbors.extend((callee, "callee") for callee in function.getCalledFunctions(monitor))
    except Exception:
        pass
    try:
        neighbors.extend((caller, "caller") for caller in function.getCallingFunctions(monitor))
    except Exception:
        pass
    for neighbor, relation in neighbors:
        key = neighbor.getEntryPoint().getOffset()
        reason = "%s of %s" % (relation, function.getEntryPoint())
        if key not in selected:
            selected[key] = {
                "function": neighbor,
                "depth": depth + 1,
                "reasons": [reason],
            }
            queue.append((neighbor, depth + 1))
        elif reason not in selected[key]["reasons"]:
            selected[key]["reasons"].append(reason)

emit("GREE RTL8720CF 0x44 -> 0x04 -> 0x47 handshake audit")
emit("label=%s" % LABEL)
emit("program=%s" % currentProgram.getName())
emit("image_base=%s" % currentProgram.getImageBase())
emit("language=%s" % currentProgram.getLanguage().getLanguageID())
emit("roots=%s" % ",".join("0x%08X" % value for value in ROOT_VALUES))
emit("call_graph_depth=%d" % DEPTH)
emit()

emit("=== TARGET FUNCTION INDEX ===")
for key in sorted(selected.keys()):
    item = selected[key]
    function = item["function"]
    callers = []
    callees = []
    try:
        callers = sorted(str(value.getEntryPoint()) for value in function.getCallingFunctions(monitor))
    except Exception:
        pass
    try:
        callees = sorted(str(value.getEntryPoint()) for value in function.getCalledFunctions(monitor))
    except Exception:
        pass
    small = sorted(value for value in function_scalars(function) if value <= 0xFF)
    emit("%s name=%s depth=%d" % (function.getEntryPoint(), function.getName(), item["depth"]))
    emit("  reasons=%s" % " || ".join(item["reasons"]))
    emit("  callers=%s" % " ".join(callers))
    emit("  callees=%s" % " ".join(callees))
    emit("  byte_scalars=%s" % " ".join("%02X" % value for value in small))
emit()

emit("=== COMMAND-CONSTANT INSTRUCTIONS ===")
interesting = set([0x04, 0x44, 0x47, 0x7E, 0x0C, 0x0D, 0x18, 0x1A])
for key in sorted(selected.keys()):
    function = selected[key]["function"]
    hits = []
    iterator = listing.getInstructions(function.getBody(), True)
    while iterator.hasNext():
        instruction = iterator.next()
        values = set()
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
        matched = sorted(interesting.intersection(values))
        if matched:
            hits.append((instruction, matched))
    if hits:
        emit("-- %s %s --" % (function.getEntryPoint(), function.getName()))
        for instruction, matched in hits:
            emit(
                "%s constants=%s  %s"
                % (
                    instruction.getAddress(),
                    ",".join("0x%02X" % value for value in matched),
                    instruction,
                )
            )
emit()

emit("=== FULL DISASSEMBLY ===")
for key in sorted(selected.keys()):
    function = selected[key]["function"]
    emit("----- %s %s -----" % (function.getEntryPoint(), function.getName()))
    iterator = listing.getInstructions(function.getBody(), True)
    while iterator.hasNext():
        instruction = iterator.next()
        emit(
            "%s  %-15s  %s"
            % (
                instruction.getAddress(),
                " ".join("%02X" % (value & 0xFF) for value in instruction.getBytes()),
                instruction,
            )
        )
    emit()

emit("=== DECOMPILED FUNCTIONS ===")
decompiler = DecompInterface()
decompiler.toggleCCode(True)
decompiler.toggleSyntaxTree(True)
decompiler.setSimplificationStyle("decompile")
decompiler.openProgram(currentProgram)
for key in sorted(selected.keys()):
    function = selected[key]["function"]
    emit("----- %s %s -----" % (function.getEntryPoint(), function.getName()))
    try:
        result = decompiler.decompileFunction(function, 180, monitor)
        if result is None or not result.decompileCompleted():
            message = "unknown"
            if result is not None:
                message = result.getErrorMessage()
            emit("DECOMPILE FAILED: %s" % message)
        else:
            emit(result.getDecompiledFunction().getC())
    except Exception as exc:
        emit("DECOMPILE EXCEPTION: %s" % exc)
    emit()

decompiler.dispose()
out.close()
print("Wrote targeted RTL8720CF handshake audit to %s" % OUT_PATH)
