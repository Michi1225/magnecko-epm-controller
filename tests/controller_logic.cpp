#include "FSM.hpp"
#include "MovingAverage.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <new>

static unsigned allocations = 0;
void *operator new(std::size_t size) {
    ++allocations;
    if (void *p = std::malloc(size)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

int main() {
    MovingAverage<20> filter;
    assert(filter.add(20) == 20); // Startup divides by samples received.
    for (int i = 0; i < 19; ++i) assert(filter.add(20) == 20);
    for (int i = 1; i <= 20; ++i)
        assert(std::fabs(filter.add(0) - (20 - i)) < 0.0001f);
    assert(filter.add(0) == 0); // Old force fully leaves the window.

    FSM fsm;
    auto action = [](FSMStatus state, uint16_t &, int8_t &) { return state; };
    FSMActions actions;
    actions.background_ = actions.notReadyToSwitchOn_ = actions.switchOnDisabled_ = action;
    actions.readyToSwitchOn_ = actions.switchedOn_ = actions.operationEnabled_ = action;
    actions.quickStopActive_ = actions.faultReactionActive_ = actions.fault_ = action;
    fsm.init(actions);
    const auto before = allocations;
    for (unsigned repeat = 0; repeat < 1000; ++repeat) {
        fsm.setControlWord(0); fsm.run();
        assert((fsm.getStatusWord() & FSMStatusWord::MASK) == 0x50);
        fsm.setControlWord(6); fsm.run();
        assert(fsm.state() == FSMStatus::READY_TO_SWITCH_ON);
        fsm.setControlWord(7); fsm.run();
        assert(fsm.state() == FSMStatus::SWITCHED_ON);
        fsm.setControlWord(15); fsm.run();
        assert((fsm.getStatusWord() & FSMStatusWord::MASK) == 0x37);
        fsm.setControlWord(2); fsm.run();
        assert(fsm.state() == FSMStatus::QUICK_STOP_ACTIVE);
        fsm.setControlWord(0); fsm.run();
    }
    fsm.triggerFaultReaction(ErrorCodes::OVER_TEMPERATURE); fsm.run();
    assert(fsm.state() == FSMStatus::FAULT_REACTION_ACTIVE);
    fsm.exitFaultReaction(); fsm.run();
    assert(fsm.state() == FSMStatus::FAULT);
    fsm.setControlWord(0x80); fsm.run();
    assert(fsm.state() == FSMStatus::SWITCH_ON_DISABLED);
    assert(allocations == before); // Includes transitions and std::function calls.
}
