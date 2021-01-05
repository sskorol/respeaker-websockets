#include "verbose.h"

#include <stdarg.h>

static char *get_time_stamp(void);

static VERBOSE LEVEL = V_NORMAL;

void setVerbose(VERBOSE lvl)
{
    LEVEL = lvl;
}

int verbose(VERBOSE lvl, FILE *stream, const char *__restrict format, ...)
{
    if (lvl > LEVEL)
        return 0;

    char *time_temp = get_time_stamp();
    char label[50];

    if (lvl == V_NORMAL)
        strcpy(label, RED " [Error] " NONE);
    if (lvl == VV_INFO)
        strcpy(label, BLUE " [Info] " NONE);
    if (lvl == VVV_DEBUG)
        strcpy(label, YELLOW " [Debug] " NONE);

    fputs(time_temp, stream);
    fputs(label, stream);

    va_list args;
    va_start(args, format);
    int ret = vfprintf(stream, format, args);
    va_end(args);
    fputs("\n", stream);
    return ret;
}

static char *get_time_stamp(void)
{
    static char c_time[50];
    time_t tmpcal_ptr;
    struct tm *tmp_ptr = NULL;
    time(&tmpcal_ptr);
    tmp_ptr = gmtime(&tmpcal_ptr);
    sprintf(c_time, GREEN "[%02d:%02d:%02d]" NONE, tmp_ptr->tm_hour, tmp_ptr->tm_min, tmp_ptr->tm_sec);
    return c_time;
}
