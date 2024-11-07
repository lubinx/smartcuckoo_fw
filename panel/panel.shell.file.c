#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "mplayer.h"
#include "sh/ucsh.h"

/****************************************************************************
 * @implements
 ****************************************************************************/
static void join_cmd(struct UCSH_env *env)
{
    int len = 0;
    for (int i = 0; i < env->argc; i ++)
        len += sprintf(&env->buf[len], "%s ", env->argv[i]);

    env->buf[len - 1] = '\0';
}

static int line_cb(char const *line, void *arg)
{
    UCSH_printf((void *)arg, "%s\n", line);
    return 0;
}

int UCSH_pwd(struct UCSH_env *env)
{
    join_cmd(env);
    return mplayer_commnad_cb(env->buf, line_cb, env);
}

int UCSH_chdir(struct UCSH_env *env)
{
    /*
    join_cmd(env);
    return mplayer_commnad_cb(env->buf, line_cb, env);
    */
    // chdir not work on R11 side
    ARG_UNUSED(env);
    return EOPNOTSUPP;
}

int UCSH_ls(struct UCSH_env *env)
{
    join_cmd(env);
    return mplayer_commnad_cb(env->buf, line_cb, env);
}

int UCSH_unlink(struct UCSH_env *env)
{
    join_cmd(env);
    return mplayer_commnad_cb(env->buf, NULL, NULL);
}

int UCSH_cat(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        int retval;

        // open
        sprintf(env->buf, "fopen %s r\r\n", env->argv[1]);
        retval = mplayer_commnad_cb(env->buf, NULL, NULL);
        if (0 != retval)
            return retval;

        // read until 0
        while (1)
        {
            sprintf(env->buf, "fread %s 128\r\n", env->argv[1]);
            retval = mplayer_commnad_cb(env->buf, NULL, NULL);

            if (0 != retval)
            {
                mplayer_recvbuf(env->buf, (size_t)retval);
                writebuf(env->fd, env->buf, (size_t)retval);
            }
            else
                break;
        }

        // close
        sprintf(env->buf, "fclose %s\r\n", env->argv[1]);
        UCSH_puts(env, "\n");

        return 0;
    }
    else
        return EINVAL;
}

int UCSH_fopen(struct UCSH_env *env)
{
    if (3 == env->argc)
    {
        sprintf(env->buf, "fopen %s %s\r\n", env->argv[1], env->argv[2]);
        return mplayer_commnad_cb(env->buf, NULL, NULL);
    }
    else
        return EINVAL;
}

int UCSH_fclose(struct UCSH_env *env)
{
    if (2 == env->argc)
    {
        join_cmd(env);
        return mplayer_commnad_cb(env->buf, NULL, NULL);
    }
    else
        return EINVAL;
}

int UCSH_fseek(struct UCSH_env *env)
{
    if (4 == env->argc)
    {
        sprintf(env->buf, "fseek %s %s\r\n", env->argv[1], env->argv[2]);
        return mplayer_commnad_cb(env->buf, NULL, NULL);
    }
    else
        return EINVAL;
}

int UCSH_fread(struct UCSH_env *env)
{
    if (3 == env->argc)
    {
        join_cmd(env);
        int retval = mplayer_commnad_cb(env->buf, NULL, NULL);
        UCSH_printf(env, "%d\n", retval);

        if (0 != retval)
        {
            mplayer_recvbuf(env->buf, (size_t)retval);
            writebuf(env->fd, env->buf, (size_t)retval);
        }
        return 0;
    }
    else
        return EINVAL;
}

int UCSH_fwrite(struct UCSH_env *env)
{
    if (3 == env->argc)
    {
        join_cmd(env);
        int retval = mplayer_commnad_cb(env->buf, NULL, NULL);
        UCSH_printf(env, "%d\n", retval);

        if (0 == retval)
        {
            int len = strtol(env->argv[2], NULL, 10);

            while (0 != len)
            {
                int readed = read(env->fd, env->buf, (size_t)(len > env->bufsize ? env->bufsize : len));
                if (0 >= readed)
                {
                    retval = errno;
                    break;
                }
                else
                {
                    mplayer_sendbuf(env->buf, (size_t)readed);
                    len -= readed;
                }
            }
        }
        return retval;
    }
    else
        return EINVAL;
}

int UCSH_freadln(struct UCSH_env *env)
{
    ARG_UNUSED(env);
    return ENOSYS;
}
