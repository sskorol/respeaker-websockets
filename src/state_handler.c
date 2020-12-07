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
    on_disabled};

void state_machine_update(void)
{
    void *ret_val = "NONE";

    if (RUNTIME.animation_enable[RUNTIME.curr_state])
    {
        verbose(VVV_DEBUG, stdout, "State is changed to %d", RUNTIME.curr_state);
        // block until the previous terminate
        pthread_join(RUNTIME.curr_thread, &ret_val);
        verbose(VVV_DEBUG, stdout, "Previous thread " PURPLE "%s" NONE " terminated with success", (char *)ret_val);
        pthread_create(&RUNTIME.curr_thread, NULL, state_functions[RUNTIME.curr_state], NULL);
    }
    else
    {
        RUNTIME.if_update = 0;
    }
}
