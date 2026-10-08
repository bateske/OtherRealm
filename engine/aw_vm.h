// SPDX-License-Identifier: GPL-2.0-or-later
// Otherrealm: bounded, allocation-free Another World bytecode interpreter.
// Compatibility work based on Raw/RawGL by Gregory Montoir (2004-2005),
// and the annotated interpreter by Fabien Sanglard. Rewritten October 2026.
#ifndef OTHERREALM_AW_VM_H
#define OTHERREALM_AW_VM_H

#include <stddef.h>
#include <stdint.h>

namespace otherrealm {

struct VmInput {
    enum Direction { Right = 1, Left = 2, Down = 4, Up = 8 };
    uint8_t directions;
    bool action;
    uint8_t character;
    VmInput() : directions(0), action(false), character(0) {}
};

// Callbacks run synchronously. updateResource must defer a part change until
// tick() returns: the current code resource must remain valid for that tick.
// present receives the original game's requested frame period (50 Hz slices).
class VmHost {
public:
    virtual ~VmHost() {}
    // The host supplies an SD-backed cache; no bytecode segment lives in RAM.
    virtual uint8_t readCode(uint16_t offset) = 0;
    virtual void drawShape(bool secondary, uint16_t offset, int16_t x,
                           int16_t y, uint16_t zoom) = 0;
    virtual void setPalette(uint8_t palette) = 0;
    virtual void selectPage(uint8_t page) = 0;
    virtual void fillPage(uint8_t page, uint8_t color) = 0;
    virtual void copyPage(uint8_t source, uint8_t destination, int16_t scroll) = 0;
    virtual void present(uint8_t page, uint16_t delayMs) = 0;
    virtual void drawString(uint16_t id, uint8_t x, uint8_t y, uint8_t color) = 0;
    virtual void drawNativeText(uint16_t, uint8_t, uint8_t, uint8_t) {}
    virtual void systemEvent(uint8_t) {}
    virtual void updateResource(uint16_t id) = 0;
    // Silent by default. Music callbacks may maintain script synchronization,
    // but this VM never opens an audio device or generates samples.
    virtual void sound(uint16_t, uint8_t, uint8_t, uint8_t) {}
    virtual void music(uint16_t, uint16_t, uint8_t) {}
};

class Vm {
public:
    enum { TaskCount = 64, VariableCount = 256, StackSize = 64 };
    enum { InactiveTask = 0xffff, RemoveTask = 0xfffe };
    explicit Vm(VmHost &host);

    // The host switches code resources before calling reset. Part changes
    // preserve variables by default, as required by the original game scripts.
    bool reset(size_t size, uint16_t part,
               bool preserveVariables = true);
    void resetVariables(uint16_t seed = 0x1234);
    bool tick(const VmInput &input, uint32_t instructionBudget = 250000);

    int16_t &variable(uint8_t index) { return variables_[index]; }
    int16_t variable(uint8_t index) const { return variables_[index]; }
    uint16_t part() const { return part_; }
    uint16_t taskPc(uint8_t task) const { return task < TaskCount ? pc_[task] : InactiveTask; }
    uint32_t instructionsLastTick() const { return instructions_; }
    uint32_t tickCount() const { return ticks_; }
    const char *error() const { return error_; }
    uint32_t faultPc() const { return faultPc_; }
    uint8_t faultTask() const { return currentTask_; }
    uint8_t faultOpcode() const { return opcode_; }

private:
    VmHost &host_;
    size_t size_;
    uint16_t part_;
    int16_t variables_[VariableCount];
    uint16_t pc_[TaskCount], nextPc_[TaskCount], calls_[StackSize];
    uint8_t state_[TaskCount], nextState_[TaskCount];
    uint8_t stack_, currentTask_, opcode_;
    uint32_t cursor_, faultPc_, instructions_, ticks_;
    bool yielded_, suspended_;
    const char *error_;

    uint8_t byte();
    uint16_t word();
    bool jump(uint16_t address);
    void fail(const char *message);
    void updateInput(const VmInput &input);
    void execute(uint8_t opcode);
    void shape(uint8_t opcode);
};

} // namespace otherrealm
#endif
