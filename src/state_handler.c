#include "animation.h"
#include "state_handler.h"
#include "verbose.h"

extern RUNTIME_OPTIONS RUNTIME;

static void *(*state_functions[STATE_NUM])() = {
    on_idle,
    on_listen,
    on_speak,
    to_mute,
    to_unmute,
    on_disabled,
    on_wake};

// Tracks which state's thread is currently running so we never pthread_join a thread
// whose `while (curr_state == X)` guard is still true. Re-entering the same live state
// would block that join forever (the thread can't see a state change that never happened)
// and freeze the whole LED/audio loop. STATE_NUM = "no thread started yet".
static STATE running_state = STATE_NUM;

void state_machine_update(void)
{
    void *ret_val = "NONE";

    // Deadlock backstop: target == currently-running state → nothing to swap.
    if (RUNTIME.curr_state == running_state)
    {
        RUNTIME.if_update = 0;
        return;
    }

    if (RUNTIME.animation_enable[RUNTIME.curr_state])
    {
        verbose(VVV_DEBUG, stdout, "State is changed to %d", RUNTIME.curr_state);
        // Block until the previous animation thread terminates. Safe now: curr_state has
        // already moved off the old state, so the old thread's while-guard goes false.
        if (running_state != STATE_NUM)
        {
            pthread_join(RUNTIME.curr_thread, &ret_val);
            verbose(VVV_DEBUG, stdout, "Previous thread " PURPLE "%s" NONE " terminated with success", (char *)ret_val);
        }
        pthread_create(&RUNTIME.curr_thread, NULL, state_functions[RUNTIME.curr_state], NULL);
        running_state = RUNTIME.curr_state;
    }
    else
    {
        RUNTIME.if_update = 0;
    }
}
