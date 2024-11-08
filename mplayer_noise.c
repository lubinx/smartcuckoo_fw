#include <ultracore/glist.h>
#include "smartcuckoo.h"

/****************************************************************************
 * @def
 ****************************************************************************/
struct NOISE_context
{
    bool playing;
    uint16_t curr_id;

    uint16_t noise_cnt;
    uint32_t noise_iter_key;
};

struct NOISE_store_t
{
    uint8_t gain[10];
    char theme[21];
};
static_assert(248 == FLASH_NVM_OBJECT_SIZE);
#define NOISE_CNT_PER_NVM               (FLASH_NVM_OBJECT_SIZE / sizeof(struct NOISE_store_t))

/****************************************************************************
 * @internal
 ****************************************************************************/
static void NOISE_discover(char *buf);
static int NOISE_shell_cmd(struct UCSH_env *env);

// var
static struct NOISE_context NOISE_context = {0};
static struct NOISE_store_t nvm_buf[NOISE_CNT_PER_NVM] = {0};

/****************************************************************************
 * @implements
 ****************************************************************************/
int NOISE_attr_init(uint16_t stored_id)
{
    NOISE_context.curr_id = stored_id;
    NOISE_context.noise_cnt = 0;

    for (char n = 'a'; n <= 'z'; n ++)
    {
        uint32_t key = NVM_DEFINE_KEY('N', 'O', 'S', n);

        if (0 == NVM_get(key, nvm_buf, sizeof(nvm_buf)))
        {
            uint8_t cnt = lengthof(nvm_buf);
            while (cnt && '\0' == nvm_buf[cnt - 1].theme[0]) cnt --;
            NOISE_context.noise_cnt += cnt;
        }
    }
    if (0 == NOISE_context.noise_cnt)
    {
        char *buf = malloc(512);
        NOISE_discover(buf);
        free(buf);
    }

    UCSH_register("nois", NOISE_shell_cmd);
    return 0;
}

bool NOISE_is_playing(void)
{
    return NOISE_context.playing;
}

static int __play(void)
{
    NOISE_stop();
    NOISE_context.playing = true;

    if (0 > (int16_t)NOISE_context.curr_id)
        NOISE_context.curr_id = NOISE_context.noise_cnt;
    else if (NOISE_context.curr_id >= NOISE_context.noise_cnt)
        NOISE_context.curr_id = 0;

    uint32_t key = NVM_DEFINE_KEY('N', 'O', 'S', 'a' + NOISE_context.curr_id / NOISE_CNT_PER_NVM);
    if (0 == NVM_get(key, nvm_buf, sizeof(nvm_buf)))
    {
        struct NOISE_store_t *nois = &nvm_buf[NOISE_context.curr_id % NOISE_CNT_PER_NVM];

        if ('\0' != nois->theme[0])
        {
            char cmd[64];
            // mnoise start JAPANGARDEN  0 0 0 0 10 0 0 0 0 0
            sprintf(cmd, "mnoise start %s %u %u %u %u %u %u %u %u %u %u", nois->theme,
                nois->gain[0], nois->gain[1], nois->gain[2], nois->gain[3], nois->gain[4],
                nois->gain[5], nois->gain[6], nois->gain[7], nois->gain[8], nois->gain[9]);

            mplayer_commnad_cb(cmd, NULL, NULL);
            return 0;
        }
    }
    return ENOENT;
}

int NOISE_toggle(void)
{
    NOISE_context.playing = ! NOISE_context.playing;

    if (NOISE_context.playing)
        return __play();
    else
        return NOISE_stop();
}

int NOISE_play(uint16_t id)
{
    NOISE_context.curr_id = id;
    return __play();
}

int NOISE_stop(void)
{
    NOISE_context.playing = false;
    return mplayer_commnad_cb("mnoise stop", NULL, NULL);
}

uint16_t NOISE_next(void)
{
    if (NOISE_context.playing)
    {
        NOISE_context.curr_id ++;
        __play();
    }
    return NOISE_context.curr_id;
}

uint16_t NOISE_prev(void)
{
    if (NOISE_context.playing)
    {
        NOISE_context.curr_id --;
        __play();
    }
    return NOISE_context.curr_id;
}

/****************************************************************************
 * @internal
 ****************************************************************************/
static int noise_ls_callback(char const *line, glist_t *list)
{
    char const *ptr = &line[0];
    while (1)
    {
        while (*ptr && '/' != *ptr) ptr ++;
        if (! *ptr)
            break;
        else
            ptr ++;

        char *tmp = (void *)ptr;
        int bytes;

        while (*tmp && '.' != *tmp) tmp ++;
        if (0 >= (bytes = tmp - ptr)) break;

        struct glist_hdr_t *hdr = (void *)malloc(sizeof(struct glist_hdr_t) + (unsigned)bytes + 1);
        if (! hdr)
            return ENOMEM;
        else
            glist_push_back(list, hdr);

        tmp = (char *)(hdr + 1);
        memcpy(tmp, ptr, (unsigned)bytes);
        tmp[bytes] = '\0';

        while (*ptr && ' ' != *ptr) ptr ++;
        while (*ptr && ' ' == *ptr) ptr ++;
        if (! *ptr) break;
    }
    return 0;
}

static void noise_store(char const *theme, char *param)
{
    uint32_t key = NVM_DEFINE_KEY('N', 'O', 'S', 'a' + NOISE_context.noise_cnt / NOISE_CNT_PER_NVM);
    if (key != NOISE_context.noise_iter_key)
    {
        NOISE_context.noise_iter_key = key;
        NVM_get(key, nvm_buf, sizeof(nvm_buf));
    }

    struct NOISE_store_t *nois = &nvm_buf[NOISE_context.noise_cnt % NOISE_CNT_PER_NVM];

    memset(nois, 0, sizeof(*nois));
    strncpy(nois->theme, theme, sizeof(nois->theme));

    uint8_t *gain = nois->gain;
    while (*param)
    {
        char const *tmp = param;
        while (*param && ',' != *param) param ++;
        if (*param)
            *param ++ = '\0';

        *gain ++ = (uint8_t)strtoul(tmp, NULL, 10);
        if (lengthof(nois->gain) == gain - nois->gain)
            break;
    }

    if (++ NOISE_context.noise_cnt == NOISE_CNT_PER_NVM)
    {
        NOISE_context.noise_iter_key = 0;
        NVM_set(key, nvm_buf, sizeof(nvm_buf));
    }

    printf("noise %s %u %u %u %u %u %u %u %u %u %u\n", nois->theme,
        nois->gain[0], nois->gain[1], nois->gain[2], nois->gain[3], nois->gain[4],
        nois->gain[5], nois->gain[6], nois->gain[7], nois->gain[8], nois->gain[9]);
}

static void noise_readfile(char const *theme, char *line_buf)
{
    int retval;
    char *cmd_buf = line_buf;
    line_buf += 64;

    // open
    sprintf(cmd_buf, "fopen noise/%s.txt r", theme);
    if (0 == (retval = mplayer_commnad_cb(cmd_buf, NULL, NULL)))
    {
        char *line;
        char *line_end = line_buf;

        // read until 0
        while (1)
        {
            sprintf(cmd_buf, "fread noise/%s.txt 64", theme);
            if (0 == (retval = mplayer_commnad_cb(cmd_buf, NULL, NULL)))
                break;

            mplayer_recvbuf(line_end, (size_t)retval);
            line_end[retval] = '\0';

            line = line_buf;
            while(*line && '[' != *line) line ++;
            if (! *line)
                break;
            else
                line_end = ++ line;

            while (*line_end && ! ('\n' == *line_end || ']' == *line_end)) line_end ++;
            switch (*line_end)
            {
            case '\0':
                break;

            case '\n':
                if ('\r' == *(line_end - 1)) line_end --;
                break;

            case ']':
                *line_end ++ = '\0';
                if ('\r' == *line_end) line_end ++;
                if ('\n' == *line_end) line_end ++;

                noise_store(theme, line);
                line = line_end;
                line_end = line_buf;
                while ('\0' != *line) { *line_end ++ = *line ++; }
                break;
            }
        }

        // close
        sprintf(cmd_buf, "fclose noise/%s.txt", theme);
        mplayer_commnad_cb(cmd_buf, NULL, NULL);
    }
}

static void NOISE_discover(char *buf)
{
    glist_t list;
    glist_init(&list);

    NOISE_context.noise_cnt = 0;
    NOISE_context.noise_iter_key = 0;

    sprintf(buf, "ls noise/*.txt");
    if (0 == mplayer_commnad_cb(buf, (void *)noise_ls_callback, &list))
    {
        struct glist_hdr_t *iter;
        while (NULL != (iter = glist_pop(&list)))
        {
            char *theme = (char *)(iter + 1);
            noise_readfile(theme, buf);

            free(iter);
        }

        if (0 != NOISE_context.noise_iter_key)
        {
            NVM_set(NOISE_context.noise_iter_key, nvm_buf, sizeof(nvm_buf));
            NOISE_context.noise_iter_key = 0;
        }
    }
}

static int NOISE_shell_cmd(struct UCSH_env *env)
{
    if (1 == env->argc)
    {
        NOISE_discover(env->argv[0]);
        return 0;
    }
    else if (2 == env->argc)
    {
        if (0 == strcasecmp("ON", env->argv[1]))
        {
            if (! NOISE_is_playing())
                NOISE_toggle();
        }
        else if (0 == strcasecmp("OFF", env->argv[1]))
        {
            NOISE_stop();
        }
        else if (0 == strcasecmp("NEXT", env->argv[1]))
        {
            NOISE_next();
        }
        else if (0 == strcasecmp("PREV", env->argv[1]))
        {
            NOISE_prev();
        }
        else
        {
            unsigned id = strtoul(env->argv[1], NULL, 10);
            NOISE_play((uint16_t)id);
        }

        UCSH_puts(env, "\n");
        return 0;
    }
    else
        return EINVAL;
}
