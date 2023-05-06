#include "animation.h"
#include "state_handler.h"
#include "verbose.h"

extern RUNTIME_OPTIONS RUNTIME;

typedef void *(*StateHandlerFunction)();

static StateHandlerFunction state_handlers[STATE_NUM] = {
    on_idle,
    on_listen,
    on_speak,
    on_mute,
    on_unmute,
    on_disabled
};

void state_machine_update(void)
{
    void *ret_val = "NONE";
    int pthread_result;

    if (RUNTIME.animation_enable[RUNTIME.curr_state])
    {
        verbose(VVV_DEBUG, stdout, "State is changed to %d", RUNTIME.curr_state);
        // block until the previous terminate
        pthread_join(RUNTIME.curr_thread, &ret_val);
        verbose(VVV_DEBUG, stdout, "Previous thread " PURPLE "%s" NONE " terminated with success", (char *)ret_val);
        pthread_result = pthread_create(&RUNTIME.curr_thread, NULL, state_handlers[RUNTIME.curr_state], NULL);
        
        if (pthread_result != 0)
        {
            fprintf(stderr, "Error creating thread in %s: %s\n", __FUNCTION__, strerror(pthread_result));
        }
    }
    else
    {
        RUNTIME.if_update = 0;
    }
}
