#include "verbose.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>

#define NONE "\033[m"
#define RED "\033[1;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[1;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[1;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;32;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

static VERBOSE LEVEL = V_NORMAL;

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
