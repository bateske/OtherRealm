// SPDX-License-Identifier: GPL-2.0-or-later
// Otherrealm VM, rewritten October 2026 for bounded embedded execution.
// Protocol and opcode semantics based on Raw/RawGL, copyright (C) 2004-2005
// Gregory Montoir, and Fabien Sanglard's annotated bytecode interpreter.
#include "aw_vm.h"
#include <string.h>

namespace otherrealm {

// Convert modulo 65536 explicitly, avoiding signed overflow in VM arithmetic.
static int16_t signedWord(uint32_t value) {
    const uint16_t low = static_cast<uint16_t>(value);
    return low < 0x8000 ? static_cast<int16_t>(low)
                        : static_cast<int16_t>(static_cast<int32_t>(low) - 65536);
}

Vm::Vm(VmHost &host)
    : host_(host), size_(0), part_(0), stack_(0), currentTask_(0),
      opcode_(0), cursor_(0), faultPc_(0), instructions_(0), ticks_(0),
      yielded_(false), suspended_(false), error_(0) {
    resetVariables();
    memset(pc_, 0xff, sizeof(pc_));
    memset(nextPc_, 0xff, sizeof(nextPc_));
    memset(state_, 0, sizeof(state_));
    memset(nextState_, 0, sizeof(nextState_));
}

void Vm::resetVariables(uint16_t seed) {
    memset(variables_, 0, sizeof(variables_));
    variables_[0x3c] = signedWord(seed);
    // State left by the original Amiga startup/protection program. The local
    // test disks have no protection and start directly at the game content.
    variables_[0xbc] = 0x10;
    variables_[0xc6] = 0x80;
    variables_[0xdc] = 33;
    variables_[0xf2] = 6000;
}

bool Vm::reset(size_t size, uint16_t part, bool preserveVariables) {
    error_ = 0;
    faultPc_ = cursor_ = instructions_ = ticks_ = 0;
    currentTask_ = opcode_ = stack_ = 0;
    yielded_ = false;
    suspended_ = false;
    size_ = size;
    part_ = part;
    if (!preserveVariables) resetVariables();
    memset(pc_, 0xff, sizeof(pc_));
    memset(nextPc_, 0xff, sizeof(nextPc_));
    memset(state_, 0, sizeof(state_));
    memset(nextState_, 0, sizeof(nextState_));
    if (!size || size > 65536) {
        fail("invalid bytecode segment");
        return false;
    }
    pc_[0] = 0;
    return true;
}

void Vm::fail(const char *message) {
    if (!error_) error_ = message;
    yielded_ = true;
}

uint8_t Vm::byte() {
    if (cursor_ >= size_) {
        fail("bytecode read beyond segment");
        return 0;
    }
    return host_.readCode(static_cast<uint16_t>(cursor_++));
}

uint16_t Vm::word() {
    const uint8_t hi = byte();
    const uint8_t lo = byte();
    return static_cast<uint16_t>((hi << 8) | lo);
}

bool Vm::jump(uint16_t address) {
    if (address >= size_) {
        fail("branch target beyond segment");
        return false;
    }
    cursor_ = address;
    return true;
}

void Vm::updateInput(const VmInput &input) {
    const uint8_t directions = input.directions & 15;
    int16_t horizontal = 0, vertical = 0;
    if (directions & VmInput::Right) horizontal = 1;
    if (directions & VmInput::Left) horizontal = -1;
    if (directions & VmInput::Down) vertical = 1;
    if (directions & VmInput::Up) vertical = -1;
    // E5 is script-owned in the Amiga release. DOS interpreters write input
    // there; doing that on Amiga corrupts gameplay state.
    variables_[0xfb] = vertical;
    variables_[0xfc] = horizontal;
    variables_[0xfd] = directions;
    variables_[0xfa] = input.action ? 1 : 0;
    variables_[0xfe] = directions | (input.action ? 0x80 : 0);
    if (part_ == 16009) {
        uint8_t c = input.character;
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        if (c == 0 || c == 8 || c == 13 || (c >= 'A' && c <= 'Z')) {
            variables_[0xda] = c;
        }
    }
}

bool Vm::tick(const VmInput &input, uint32_t instructionBudget) {
    if (error_) return false;
    if (!size_) { fail("no bytecode loaded"); return false; }
    updateInput(input);
    suspended_ = false;
    instructions_ = 0;
    ++ticks_;
    // Requests become visible together at the next cooperative VM tick.
    for (unsigned task = 0; task < TaskCount; ++task) {
        state_[task] = nextState_[task];
        if (nextPc_[task] != InactiveTask) {
            pc_[task] = nextPc_[task] == RemoveTask ? InactiveTask : nextPc_[task];
            nextPc_[task] = InactiveTask;
        }
    }
    for (currentTask_ = 0; currentTask_ < TaskCount; ++currentTask_) {
        if (state_[currentTask_] || pc_[currentTask_] == InactiveTask) continue;
        cursor_ = pc_[currentTask_];
        stack_ = 0;
        yielded_ = false;
        while (!yielded_) {
            faultPc_ = cursor_;
            if (instructions_ >= instructionBudget) {
                fail("instruction budget exhausted (task did not yield)");
                break;
            }
            ++instructions_;
            opcode_ = byte();
            if (error_) break;
            if (opcode_ & 0xc0) shape(opcode_);
            else execute(opcode_);
        }
        pc_[currentTask_] = static_cast<uint16_t>(cursor_);
        if (error_) return false;
        if (suspended_) break;
    }
    return true;
}

void Vm::shape(uint8_t op) {
    uint16_t offset, zoom = 64;
    int16_t x, y;
    bool secondary = false;
    if (op & 0x80) {
        offset = static_cast<uint16_t>(((op << 8) | byte()) << 1);
        x = byte();
        y = byte();
        // Compact encoding borrows excess Y values to reach X beyond 255.
        if (y > 199) { x += y - 199; y = 199; }
    } else {
        offset = static_cast<uint16_t>(word() << 1);
        const uint8_t firstX = byte();
        if (!(op & 0x20)) {
            x = (op & 0x10) ? variables_[firstX]
                           : signedWord((firstX << 8) | byte());
        } else {
            x = firstX + ((op & 0x10) ? 256 : 0);
        }
        const uint8_t firstY = byte();
        if (op & 8) y = firstY;
        else if (op & 4) y = variables_[firstY];
        else y = signedWord((firstY << 8) | byte());
        switch (op & 3) {
        case 1: zoom = static_cast<uint16_t>(variables_[byte()]); break;
        case 2: zoom = byte(); break;
        case 3: secondary = true; break;
        default: break;
        }
    }
    if (!error_) host_.drawShape(secondary, offset, x, y, zoom);
}

void Vm::execute(uint8_t op) {
    switch (op) {
    case 0x00: {
        const uint8_t dst = byte(); const uint16_t value = word();
        if (!error_) variables_[dst] = signedWord(value);
        break;
    }
    case 0x01: case 0x02: case 0x13: {
        const uint8_t dst = byte(), src = byte();
        if (error_) break;
        if (op == 1) variables_[dst] = variables_[src];
        else if (op == 2) variables_[dst] = signedWord(static_cast<int32_t>(variables_[dst]) + variables_[src]);
        else variables_[dst] = signedWord(static_cast<int32_t>(variables_[dst]) - variables_[src]);
        break;
    }
    case 0x03: case 0x14: case 0x15: case 0x16: case 0x17: {
        const uint8_t dst = byte(); const uint16_t operand = word();
        if (error_) break;
        const uint16_t value = static_cast<uint16_t>(variables_[dst]);
        if (op == 3) variables_[dst] = signedWord(static_cast<uint32_t>(value) + operand);
        else if (op == 0x14) variables_[dst] = signedWord(value & operand);
        else if (op == 0x15) variables_[dst] = signedWord(value | operand);
        else if (op == 0x16) variables_[dst] = operand >= 16 ? 0 : signedWord(static_cast<uint32_t>(value) << operand);
        else variables_[dst] = operand >= 16 ? 0 : signedWord(value >> operand);
        break;
    }
    case 0x04: {
        const uint16_t target = word();
        if (error_) break;
        if (stack_ == StackSize) { fail("call stack overflow"); break; }
        calls_[stack_++] = static_cast<uint16_t>(cursor_);
        jump(target);
        break;
    }
    case 0x05:
        if (!stack_) fail("call stack underflow");
        else jump(calls_[--stack_]);
        break;
    case 0x06: yielded_ = true; break;
    case 0x07: { const uint16_t target = word(); if (!error_) jump(target); break; }
    case 0x08: {
        const uint8_t task = byte(); const uint16_t target = word();
        if (error_) break;
        if (task >= TaskCount) fail("invalid task index");
        else if (target != InactiveTask && target != RemoveTask && target >= size_) fail("installed task beyond segment");
        else nextPc_[task] = target;
        break;
    }
    case 0x09: {
        const uint8_t var = byte(); const uint16_t target = word();
        if (error_) break;
        variables_[var] = signedWord(static_cast<int32_t>(variables_[var]) - 1);
        if (variables_[var]) jump(target);
        break;
    }
    case 0x0a: {
        const uint8_t condition = byte(), var = byte();
        const int16_t a = variables_[var];
        int16_t b;
        if (condition & 0x80) b = variables_[byte()];
        else if (condition & 0x40) b = signedWord(word());
        else b = byte();
        const uint16_t target = word();
        if (error_) break;
        bool take = false;
        switch (condition & 7) {
        case 0: take = a == b; break;
        case 1: take = a != b; break;
        case 2: take = a > b; break;
        case 3: take = a >= b; break;
        case 4: take = a < b; break;
        case 5: take = a <= b; break;
        default: fail("invalid comparison"); break;
        }
        if (take) jump(target);
        break;
    }
    case 0x0b: { const uint16_t p = word(); if (!error_) host_.setPalette(p >> 8); break; }
    case 0x0c: {
        const uint8_t first = byte(), last = byte(), state = byte();
        if (error_) break;
        if (first >= TaskCount || last >= TaskCount || last < first || state > 2) {
            fail("invalid task state range"); break;
        }
        for (unsigned i = first; i <= last; ++i) {
            if (state == 2) nextPc_[i] = RemoveTask;
            else nextState_[i] = state;
        }
        break;
    }
    case 0x0d: { const uint8_t page = byte(); if (!error_) host_.selectPage(page); break; }
    case 0x0e: {
        const uint8_t page = byte(), color = byte();
        if (!error_) host_.fillPage(page, color);
        break;
    }
    case 0x0f: {
        const uint8_t source = byte(), destination = byte();
        if (!error_) host_.copyPage(source, destination, variables_[0xf9]);
        break;
    }
    case 0x10: {
        const uint8_t page = byte();
        if (!error_) {
            variables_[0xf7] = 0;
            const int32_t ms = static_cast<int32_t>(variables_[0xff]) * 20;
            host_.present(page, static_cast<uint16_t>(ms < 0 ? 0 : (ms > 65535 ? 65535 : ms)));
        }
        break;
    }
    case 0x11: cursor_ = InactiveTask; yielded_ = true; break;
    case 0x12: {
        const uint16_t id = word(); const uint8_t x = byte(), y = byte(), color = byte();
        if (!error_) host_.drawString(id, x, y, color);
        break;
    }
    case 0x1b: {
        const uint16_t id = word(); const uint8_t x = byte(), y = byte(), color = byte();
        if (!error_) host_.drawNativeText(id, x, y, color);
        break;
    }
    case 0x1c: {
        const uint8_t event = byte();
        uint8_t reserved = 0;
        for (unsigned i=0;i<4;++i) reserved |= byte();
        if (!error_ && (reserved || event>3)) fail("invalid system event");
        if (!error_ && event) {host_.systemEvent(event); suspended_=yielded_=true;}
        break;
    }
    case 0x18: {
        const uint16_t id = word(); const uint8_t frequency = byte(), volume = byte(), channel = byte();
        if (!error_) host_.sound(id, frequency, volume, channel);
        break;
    }
    case 0x19: { const uint16_t id = word(); if (!error_) host_.updateResource(id); break; }
    case 0x1a: {
        const uint16_t id = word(), delay = word(); const uint8_t position = byte();
        if (!error_) host_.music(id, delay, position);
        break;
    }
    default: fail("unknown opcode"); break;
    }
}

} // namespace otherrealm
