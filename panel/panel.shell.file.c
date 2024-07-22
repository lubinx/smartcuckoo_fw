#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "mplayer.h"
#include "sh/ucsh.h"

/****************************************************************************
 * @def
 ****************************************************************************/
#define BLKSIZE                         (128)

/*
    fwrite <filename> <len> [append|trunc]\r\n      return 0 - errno
    fseek <filename> <0/1/2> <pos>
    fread  <filename> <len>\r\n                     return len
    fclose <filename>
*/

/****************************************************************************
 * @implements
 ****************************************************************************/
int FILE_open(struct UCSH_env *env)
{
    if (3 != env->argc)
        return EINVAL;
    if (0 != strcmp("append", env->argv[2])  && 0 != strcmp("trunc", env->argv[2]))
        return EINVAL;

    sprintf(env->buf, "fopen %s %s\r\n", env->argv[1], env->argv[2]);
        return mplayer_commnad_cb(env->buf, NULL, NULL);
}

int FILE_seek(struct UCSH_env *env)
{
    if (3 != env->argc)
        return EINVAL;

    sprintf(env->buf, "fseek %s %s\r\n", env->argv[1], env->argv[2]);
        return mplayer_commnad_cb(env->buf, NULL, NULL);
}
