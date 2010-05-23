// comsys.cpp
//
// * $Id: comsys.cpp,v 1.51 2002/07/18 00:35:55 sdennis Exp $
//
#include "copyright.h"
#include "autoconf.h"
#include "config.h"
#include "externs.h"

#include <sys/types.h>

#include "ansi.h"
#include "db.h"
#include "interface.h"
#include "attrs.h"
#include "match.h"
#include "config.h"
#include "flags.h"
#include "powers.h"
#include "functions.h"

#include "comsys.h"

int num_channels;
int max_channels;
struct channel **channels;
comsys_t *comsys_table[NUM_COMSYS];

// Return value must be free_lbuf'ed.
//
char *RestrictTitleValue(char *pTitleRequest)
{
    // First, remove all '\r\n\t' from the string.
    //
    char *pNewTitle = RemoveSetOfCharacters(pTitleRequest, "\r\n\t");

    // Optimize/terminate any ANSI in the string.
    //
    char NewTitle_ANSI[MAX_TITLE_LEN+1];
    int nVisualWidth;
    int nLen = ANSI_TruncateToField(pNewTitle, sizeof(NewTitle_ANSI),
        NewTitle_ANSI, sizeof(NewTitle_ANSI), &nVisualWidth,
        ANSI_ENDGOAL_NORMAL);
    memcpy(pNewTitle, NewTitle_ANSI, nLen+1);
    return pNewTitle;
}

void do_setnewtitle(dbref player, struct channel *ch, char *pValidatedTitle)
{
    struct comuser *user = select_user(ch, player);

    if (ch && user)
    {
        if (user->title)
        {
            MEMFREE(user->title);
        }
        user->title = StringClone(pValidatedTitle);
    }
}

void load_comsys(char *filename)
{
    int i;
    char buffer[200];

    for (i = 0; i < NUM_COMSYS; i++)
    {
        comsys_table[i] = NULL;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        Log.tinyprintf("Error: Couldn't find %s." ENDLINE, filename);
    }
    else
    {
        DebugTotalFiles++;
        Log.tinyprintf("LOADING: %s" ENDLINE, filename);
        if (fscanf(fp, "*** Begin %s ***\n", buffer) == 1 && !strcmp(buffer, "COMMAC"))
        {
            load_old_channels(fp);
        }
        else if (!strcmp(buffer, "CHANNELS"))
        {
            load_channels(fp);
        }
        else
        {
            Log.tinyprintf("Error: Couldn't find Begin CHANNELS in %s.", filename);
            return;
        }

        if (fscanf(fp, "*** Begin %s ***\n", buffer) == 1 && !strcmp(buffer, "COMSYS"))
        {
            load_comsystem(fp);
        }
        else
        {
            Log.tinyprintf("Error: Couldn't find Begin COMSYS in %s.", filename);
            return;
        }

        if (fclose(fp) == 0)
        {
            DebugTotalFiles--;
        }
        Log.tinyprintf("LOADING: %s (done)" ENDLINE, filename);
    }
}

void save_comsys(char *filename)
{
    char buffer[500];

    sprintf(buffer, "%s.#", filename);
    FILE *fp = fopen(buffer, "wb");
    if (!fp)
    {
        Log.tinyprintf("Unable to open %s for writing." ENDLINE, buffer);
        return;
    }
    DebugTotalFiles++;
    fprintf(fp, "*** Begin CHANNELS ***\n");
    save_channels(fp);

    fprintf(fp, "*** Begin COMSYS ***\n");
    save_comsystem(fp);

    if (fclose(fp) == 0)
    {
        DebugTotalFiles--;
    }
    ReplaceFile(buffer, filename);
}

// Aliases must be between 1 and 5 characters. No spaces. No ANSI.
//
char *MakeCanonicalComAlias(const char *pAlias, int *nValidAlias, BOOL *bValidAlias)
{
    static char Buffer[6];
    *nValidAlias = 0;
    *bValidAlias = FALSE;

    if (!pAlias)
    {
        return NULL;
    }
    const char *p = pAlias;
    char *q = Buffer;
    int n = 0;
    while (*p)
    {
        if (!Tiny_IsPrint[(unsigned char)*p] || *p == ' ')
        {
            return NULL;
        }
        if (n <= 5)
        {
            n++;
            *q++ = *p;
        }
        p++;
    }
    *q = '\0';
    if (n < 1 || 5 < n)
    {
        return FALSE;
    }
    *nValidAlias = n;
    *bValidAlias = TRUE;
    return Buffer;
}

BOOL ParseChannelLine(char *pBuffer, char *pAlias5, char **ppChannelName)
{
    // Fetch alias portion. We need to find the first space.
    //
    char *p = strchr(pBuffer, ' ');
    if (!p) return FALSE;

    *p = '\0';
    BOOL bValidAlias;
    int  nValidAlias;
    char *pValidAlias = MakeCanonicalComAlias(pBuffer, &nValidAlias, &bValidAlias);
    if (!bValidAlias)
    {
        return FALSE;
    }
    strcpy(pAlias5, pValidAlias);

    // Skip any leading space before the channel name.
    //
    p++;
    while (Tiny_IsSpace[(unsigned char)*p])
    {
        p++;
    }

    if (*p == '\0')
    {
        return FALSE;
    }

    // The rest of the line is the channel name.
    //
    *ppChannelName = StringClone(p);
    return TRUE;
}

void load_channels(FILE *fp)
{
    int i, j;
    char buffer[LBUF_SIZE];
    int np;
    comsys_t *c;

    fscanf(fp, "%d\n", &np);
    for (i = 0; i < np; i++)
    {
        c = create_new_comsys();
        fscanf(fp, "%d %d\n", &(c->who), &(c->numchannels));
        c->maxchannels = c->numchannels;
        if (c->maxchannels > 0)
        {
            c->alias = (char *)MEMALLOC(c->maxchannels * 6);
            (void)ISOUTOFMEMORY(c->alias);
            c->channels = (char **)MEMALLOC(sizeof(char *) * c->maxchannels);
            (void)ISOUTOFMEMORY(c->channels);

            for (j = 0; j < c->numchannels; j++)
            {
                int n = GetLineTrunc(buffer, sizeof(buffer), fp);
                if (buffer[n-1] == '\n')
                {
                    // Get rid of trailing '\n'.
                    //
                    n--;
                    buffer[n] = '\0';
                }
                if (!ParseChannelLine(buffer, c->alias + j * 6, c->channels+j))
                {
                    c->numchannels--;
                    j--;
                }
            }
            sort_com_aliases(c);
        }
        else
        {
            c->alias = NULL;
            c->channels = NULL;
        }
        if (c->who >= 0 && c->who < mudstate.db_top)
        {
            if (  isPlayer(c->who)
               || !God(Owner(c->who))
               || !Going(c->who))
            {
                add_comsys(c);
            }
        }
        else
        {
            Log.tinyprintf("dbref %d out of range [0, %d)" ENDLINE, c->who, mudstate.db_top);
        }
        purge_comsystem();
    }
}

void load_old_channels(FILE *fp)
{
    int i, j, k;
    char buffer[LBUF_SIZE];
    int np;
    comsys_t *c;
    char *t;
    char in;

    fscanf(fp, "%d\n", &np);
    for (i = 0; i < np; i++)
    {
        c = create_new_comsys();
        // Trash the old values!
        fscanf(fp, "%d %d %d %d %d %d %d %d\n", &(c->who), &(c->numchannels), &k, &k, &k, &k, &k, &k);
        c->maxchannels = c->numchannels;
        if (c->maxchannels > 0)
        {
            c->alias = (char *)MEMALLOC(c->maxchannels * 6);
            (void)ISOUTOFMEMORY(c->alias);
            c->channels = (char **)MEMALLOC(sizeof(char *) * c->maxchannels);
            (void)ISOUTOFMEMORY(c->channels);

            for (j = 0; j < c->numchannels; j++)
            {
                t = c->alias + j * 6;
                while ((in = fgetc(fp)) != ' ')
                {
                    *t++ = in;
                }
                *t = 0;

                int n = GetLineTrunc(buffer, sizeof(buffer), fp);
                if (buffer[n-1] == '\n')
                {
                    // Get rid of trailing '\n'.
                    //
                    n--;
                }
                c->channels[j] = StringCloneLen(buffer, n);
            }
            sort_com_aliases(c);
        }
        else
        {
            c->alias = NULL;
            c->channels = NULL;
        }
        if (c->who >= 0 && c->who < mudstate.db_top)
        {
            if (  isPlayer(c->who)
               || !God(Owner(c->who))
               || !Going(c->who))
            {
                add_comsys(c);
            }
        }
        else
        {
            Log.tinyprintf("load_old_channels: dbref %d out of range [0, %d)" ENDLINE, c->who, mudstate.db_top);
        }
        purge_comsystem();
    }
}

void purge_comsystem(void)
{
    comsys_t *c;
    comsys_t *d;
    int i;

#ifdef ABORT_PURGE_COMSYS
    return;
#endif // ABORT_PURGE_COMSYS

    for (i = 0; i < NUM_COMSYS; i++)
    {
        c = comsys_table[i];
        while (c)
        {
            d = c;
            c = c->next;
            if (d->numchannels == 0)
            {
                del_comsys(d->who);
                continue;
            }
            if (isPlayer(d->who))
            {
                continue;
            }
            if (  God(Owner(d->who))
               && Going(d->who))
            {
                del_comsys(d->who);
                continue;
            }
        }
    }
}

void save_channels(FILE *fp)
{
    int np;
    comsys_t *c;
    int i, j;

    purge_comsystem();
    np = 0;
    for (i = 0; i < NUM_COMSYS; i++)
    {
        c = comsys_table[i];
        while (c)
        {
            np++;
            c = c->next;
        }
    }

    fprintf(fp, "%d\n", np);
    for (i = 0; i < NUM_COMSYS; i++)
    {
        c = comsys_table[i];
        while (c)
        {
            fprintf(fp, "%d %d\n", c->who, c->numchannels);
            for (j = 0; j < c->numchannels; j++)
            {
                fprintf(fp, "%s %s\n", c->alias + j * 6, c->channels[j]);
            }
            c = c->next;
        }
    }
}

comsys_t *create_new_comsys(void)
{
    comsys_t *c;

    c = (comsys_t *)MEMALLOC(sizeof(comsys_t));
    (void)ISOUTOFMEMORY(c);

    c->who = -1;
    c->numchannels = 0;
    c->maxchannels = 0;
    c->alias = NULL;
    c->channels = NULL;

    c->next = NULL;
    return c;
}

comsys_t *get_comsys(dbref which)
{
    comsys_t *c;

    if (which < 0)
        return NULL;

    c = comsys_table[which % NUM_COMSYS];

    while (c && (c->who != which))
        c = c->next;

    if (!c)
    {
        c = create_new_comsys();
        c->who = which;
        add_comsys(c);
    }
    return c;
}

void add_comsys(comsys_t *c)
{
    if (c->who < 0 || c->who >= mudstate.db_top)
    {
        Log.tinyprintf("add_comsys: dbref %d out of range [0, %d)" ENDLINE, c->who, mudstate.db_top);
        return;
    }

    c->next = comsys_table[c->who % NUM_COMSYS];
    comsys_table[c->who % NUM_COMSYS] = c;
}

void del_comsys(dbref who)
{
    comsys_t *c;
    comsys_t *last;

    if (who < 0 || who >= mudstate.db_top)
    {
        Log.tinyprintf("del_comsys: dbref %d out of range [0, %d)" ENDLINE, who, mudstate.db_top);
        return;
    }

    c = comsys_table[who % NUM_COMSYS];

    if (c == NULL)
        return;

    if (c->who == who)
    {
        comsys_table[who % NUM_COMSYS] = c->next;
        destroy_comsys(c);
        return;
    }
    last = c;
    c = c->next;
    while (c)
    {
        if (c->who == who)
        {
            last->next = c->next;
            destroy_comsys(c);
            return;
        }
        last = c;
        c = c->next;
    }
}

void destroy_comsys(comsys_t *c)
{
    int i;

    if (c->alias)
    {
        MEMFREE(c->alias);
    }
    for (i = 0; i < c->numchannels; i++)
    {
        MEMFREE(c->channels[i]);
    }
    if (c->channels)
    {
        MEMFREE(c->channels);
    }
    MEMFREE(c);
}

void sort_com_aliases(comsys_t *c)
{
    int i;
    int cont;
    char buffer[10];
    char *s;

    cont = 1;
    while (cont)
    {
        cont = 0;
        for (i = 0; i < c->numchannels - 1; i++)
        {
            if (strcmp(c->alias + i * 6, c->alias + (i + 1) * 6) > 0)
            {
                strcpy(buffer, c->alias + i * 6);
                strcpy(c->alias + i * 6, c->alias + (i + 1) * 6);
                strcpy(c->alias + (i + 1) * 6, buffer);
                s = c->channels[i];
                c->channels[i] = c->channels[i + 1];
                c->channels[i + 1] = s;
                cont = 1;
            }
        }
    }
}

char *get_channel_from_alias(dbref player, char *alias)
{
    comsys_t *c;
    int first, last, current;
    int dir;

    c = get_comsys(player);

    current = 0;
    first = 0;
    last = c->numchannels - 1;
    dir = 1;

    while (dir && (first <= last))
    {
        current = (first + last) / 2;
        dir = strcmp(alias, c->alias + 6 * current);
        if (dir < 0)
            last = current - 1;
        else
            first = current + 1;
    }

    if (!dir)
        return c->channels[current];
    else
        return "";
}

void load_comsystem(FILE *fp)
{
    int i, j, dummy;
    int nc, ver = 0;
    struct channel *ch;
    char temp[LBUF_SIZE];

    num_channels = 0;

    fgets(temp, sizeof(temp), fp);
    if (!strncmp(temp, "+V", 2))
    {
        // +V2 has colored headers
        //
        ver = Tiny_atol(temp + 2);
        if (ver < 1 || 2 < ver)
        {
            return;
        }
        fscanf(fp, "%d\n", &nc);
    }
    else
    {
        nc = Tiny_atol(temp);
    }

    num_channels = nc;

    for (i = 0; i < nc; i++)
    {
        ch = (struct channel *)MEMALLOC(sizeof(struct channel));
        (void)ISOUTOFMEMORY(ch);

        int nChannel = GetLineTrunc(temp, sizeof(temp), fp);
        if (nChannel > MAX_CHANNEL_LEN)
        {
            nChannel = MAX_CHANNEL_LEN;
        }
        if (temp[nChannel-1] == '\n')
        {
            // Get rid of trailing '\n'.
            //
            nChannel--;
        }
        memcpy(ch->name, temp, nChannel);
        ch->name[nChannel] = '\0';

        if (ver == 2)
        {
            int nHeader = GetLineTrunc(temp, sizeof(temp), fp);
            if (nHeader > MAX_HEADER_LEN)
            {
                nHeader = MAX_HEADER_LEN;
            }
            if (temp[nHeader-1] == '\n')
            {
                nHeader--;
            }
            memcpy(ch->header, temp, nHeader);
            ch->header[nHeader] = '\0';
        }

        ch->on_users = NULL;

        hashaddLEN(ch->name, nChannel, (int *)ch, &mudstate.channel_htab);

        if (ver)
        {
            fscanf(fp, "%d %d %d %d %d %d %d %d\n",
                &(ch->type), &(ch->temp1), &(ch->temp2),
                &(ch->charge), &(ch->charge_who),
                &(ch->amount_col), &(ch->num_messages), &(ch->chan_obj));
        }
        else
        {
            fscanf(fp, "%d %d %d %d %d %d %d %d %d %d\n",
                &(ch->type), &(dummy), &(ch->temp1), &(ch->temp2),
                &(dummy), &(ch->charge), &(ch->charge_who),
                &(ch->amount_col), &(ch->num_messages), &(ch->chan_obj));
        }

        if (ver != 2)
        {
            // Build colored header if not +V2 db.
            //
            if (ch->type & CHANNEL_PUBLIC)
            {
                sprintf(temp, "%s[%s%s%s%s%s]%s", ANSI_CYAN, ANSI_HILITE,
                    ANSI_BLUE, ch->name, ANSI_NORMAL, ANSI_CYAN, ANSI_NORMAL);
            }
            else
            {
                sprintf(temp, "%s[%s%s%s%s%s]%s", ANSI_MAGENTA, ANSI_HILITE,
                    ANSI_RED, ch->name, ANSI_NORMAL, ANSI_MAGENTA,
                    ANSI_NORMAL);
            }
            int vwVisual;
            ANSI_TruncateToField(temp, MAX_HEADER_LEN+1, ch->header,
                MAX_HEADER_LEN+1, &vwVisual, ANSI_ENDGOAL_NORMAL);
        }

        fscanf(fp, "%d\n", &(ch->num_users));
        ch->max_users = ch->num_users;
        if (ch->num_users > 0)
        {
            ch->users = (struct comuser **)calloc(ch->max_users, sizeof(struct comuser *));

            int jAdded = 0;
            for (j = 0; j < ch->num_users; j++)
            {
                struct comuser t_user;
                memset(&t_user, 0, sizeof(t_user));

                if (ver)
                {
                    fscanf(fp, "%d %d\n", &(t_user.who), &(t_user.bUserIsOn));
                }
                else
                {
                    fscanf(fp, "%d %d %d", &(t_user.who), &(dummy), &(dummy));
                    fscanf(fp, "%d\n", &(t_user.bUserIsOn));
                }

                // Read Comtitle.
                //
                int nTitle = GetLineTrunc(temp, sizeof(temp), fp);
                char *pTitle = temp;

                if (  t_user.who >= 0
                   && t_user.who < mudstate.db_top)
                {
                    // Validate comtitle
                    //
                    if (3 < nTitle && temp[0] == 't' && temp[1] == ':')
                    {
                        pTitle = temp+2;
                        nTitle -= 2;
                        if (pTitle[nTitle-1] == '\n')
                        {
                            // Get rid of trailing '\n'.
                            //
                            nTitle--;
                        }
                        if (nTitle <= 0 || MAX_TITLE_LEN < nTitle)
                        {
                            nTitle = 0;
                            pTitle = temp;
                        }
                    }
                    else
                    {
                        nTitle = 0;
                    }

                    struct comuser *user = (struct comuser *)MEMALLOC(sizeof(struct comuser));
                    (void)ISOUTOFMEMORY(user);
                    memcpy(user, &t_user, sizeof(struct comuser));

                    user->title = StringCloneLen(pTitle, nTitle);
                    ch->users[jAdded++] = user;

                    if (  !(isPlayer(user->who))
                       && !(Going(user->who)
                       && (God(Owner(user->who)))))
                    {
                        do_joinchannel(user->who, ch);
                    }
                    user->on_next = ch->on_users;
                    ch->on_users = user;
                }
                else
                {
                    Log.tinyprintf("load_comsystem: dbref %d out of range [0, %d)" ENDLINE, t_user.who, mudstate.db_top);
                }
            }
            ch->num_users = jAdded;
            sort_users(ch);
        }
        else
        {
            ch->users = NULL;
        }
    }
}

void save_comsystem(FILE *fp)
{
    struct channel *ch;
    struct comuser *user;
    int j;

    fprintf(fp, "+V2\n");
    fprintf(fp, "%d\n", num_channels);
    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab); ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        fprintf(fp, "%s\n", ch->name);
        fprintf(fp, "%s\n", ch->header);

        fprintf(fp, "%d %d %d %d %d %d %d %d\n", ch->type, ch->temp1, ch->temp2, ch->charge, ch->charge_who, ch->amount_col, ch->num_messages, ch->chan_obj);

        // Count the number of 'valid' users to dump.
        //
        int nUsers = 0;
        for (j = 0; j < ch->num_users; j++)
        {
            user = ch->users[j];
            if (user->who >= 0 && user->who < mudstate.db_top)
            {
                nUsers++;
            }
        }

        fprintf(fp, "%d\n", nUsers);
        for (j = 0; j < ch->num_users; j++)
        {
            user = ch->users[j];
            if (user->who >= 0 && user->who < mudstate.db_top)
            {
                user = ch->users[j];
                fprintf(fp, "%d %d\n", user->who, user->bUserIsOn);
                if (user->title[0] != '\0')
                    fprintf(fp, "t:%s\n", user->title);
                else
                    fprintf(fp, "t:\n");
            }
        }
    }
}

void do_processcom(dbref player, char *arg1, char *arg2)
{
    char *mess, *bp;

    if ((strlen(arg1) + strlen(arg2)) > 3500)
    {
        arg2[3500] = '\0'; // TODO: Incorrect logic that doesn't hurt anything.
    }
    if (!*arg2)
    {
        raw_notify(player, "No message.");
        return;
    }
    struct channel *ch = select_channel(arg1);
    if (!ch)
    {
        raw_notify(player, tprintf("Unknown channel %s.", arg1));
        return;
    }
    struct comuser *user = select_user(ch, player);
    if (!user)
    {
        raw_notify(player, "You are not listed as on that channel.  Delete this alias and readd.");
        return;
    }
    if (  Gagged(player)
       && !Wizard(player))
    {
        raw_notify(player, "GAGGED players may not speak on channels.");
        return;
    }
    if (!strcmp(arg2, "on"))
    {
        do_joinchannel(player, ch);
    }
    else if (!strcmp(arg2, "off"))
    {
        do_leavechannel(player, ch);
    }
    else if (!user->bUserIsOn)
    {
        raw_notify(player, tprintf("You must be on %s to do that.", arg1));
        return;
    }
    else if (!strcmp(arg2, "who"))
    {
        do_comwho(player, ch);
    }
    else if (!(do_test_access(player, CHANNEL_TRANSMIT, ch)))
    {
        raw_notify(player, "That channel type cannot be transmitted on.");
        return;
    }
    else
    {
        if (!payfor(player, Guest(player) ? 0 : ch->charge))
        {
            notify(player, tprintf("You don't have enough %s.", mudconf.many_coins));
            return;
        }
        else
        {
            ch->amount_col += ch->charge;
            giveto(ch->charge_who, ch->charge);
        }

        bp = mess = alloc_lbuf("do_processcom");

        if ((*arg2) == ':')
        {
            if (user->title[0] != '\0')
            {
                // There is a comtitle.
                //
                if (ch->type & CHANNEL_SPOOF)
                {
                    safe_tprintf_str(mess, &bp, "%s %s %s", ch->header, user->title, arg2 + 1);
                }
                else
                {
                    safe_tprintf_str(mess, &bp, "%s %s %s %s", ch->header, user->title, Name(player), arg2 + 1);
                }
            }
            else
            {
                safe_tprintf_str(mess, &bp, "%s %s %s", ch->header, Name(player), arg2 + 1);
            }
        }
        else if ((*arg2) == ';')
        {
            if (user->title[0] != '\0')
            {
                // There is a comtitle
                //
                if (ch->type & CHANNEL_SPOOF)
                {
                    safe_tprintf_str(mess, &bp, "%s %s%s", ch->header, user->title, arg2 + 1);
                }
                else
                {
                    safe_tprintf_str(mess, &bp, "%s %s %s%s", ch->header, user->title, Name(player), arg2 + 1);
                }
            }
            else
            {
                safe_tprintf_str(mess, &bp, "%s %s%s", ch->header, Name(player), arg2 + 1);
            }
        }
        else
        {
            if (user->title[0] != '\0')
            {
                // There is a comtitle
                //
                if (ch->type & CHANNEL_SPOOF)
                {
                    safe_tprintf_str(mess, &bp, "%s %s says, \"%s\"", ch->header, user->title, arg2);
                }
                else
                {
                    safe_tprintf_str(mess, &bp, "%s %s %s says, \"%s\"", ch->header, user->title, Name(player), arg2);
                }
            }
            else
            {
                safe_tprintf_str(mess, &bp, "%s %s says, \"%s\"", ch->header, Name(player), arg2);
            }
        }

        do_comsend(ch, mess, player);
        free_lbuf(mess);
    }
}

void do_comsend(struct channel *ch, char *msgNormal, dbref player)
{
    struct comuser *user;

    ch->num_messages++;
    for (user = ch->on_users; user; user = user->on_next)
    {
        if (  user->bUserIsOn
           && do_test_access(user->who, CHANNEL_RECEIVE, ch))
        {
            notify_with_cause_ooc(user->who, player, msgNormal);
        }
    }
}

void do_joinchannel(dbref player, struct channel *ch)
{
    char *p;
    struct comuser *user;
    struct comuser **cu;
    int i;

    user = select_user(ch, player);

    if (!user)
    {
        ch->num_users++;
        if (ch->num_users >= ch->max_users)
        {
            ch->max_users += 10;
            cu = (struct comuser **)MEMALLOC(sizeof(struct comuser *) * ch->max_users);
            (void)ISOUTOFMEMORY(cu);

            for (i = 0; i < (ch->num_users - 1); i++)
            {
                cu[i] = ch->users[i];
            }
            MEMFREE(ch->users);
            ch->users = cu;
        }
        user = (struct comuser *)MEMALLOC(sizeof(struct comuser));
        (void)ISOUTOFMEMORY(user);

        for (i = ch->num_users - 1; i > 0 && ch->users[i - 1]->who > player; i--)
        {
            ch->users[i] = ch->users[i - 1];
        }
        ch->users[i] = user;

        user->who = player;
        user->bUserIsOn = 1;

        user->title = StringClone("");

        // if (Connected(player))&&(isPlayer(player))
        //
        if UNDEAD(player)
        {
            user->on_next = ch->on_users;
            ch->on_users = user;
        }
    }
    else if (!user->bUserIsOn)
    {
        user->bUserIsOn = 1;
    }
    else
    {
        raw_notify(player, tprintf("You are already on channel %s.",
            ch->name));
        return;
    }

    if (!Dark(player))
    {
        if (user->title[0] != '\0')
        {
            // There is a comtitle
            //
            if (ch->type & CHANNEL_SPOOF)
            {
                p = tprintf( "%s %s has joined this channel.", ch->header,
                    user->title);
            }
            else
            {
                p = tprintf( "%s %s %s has joined this channel.", ch->header,
                    user->title, Name(player));
            }
        }
        else
        {
            p = tprintf( "%s %s has joined this channel.", ch->header,
                Name(player));
        }
        do_comsend(ch, p, player);
    }
}

void do_leavechannel(dbref player, struct channel *ch)
{
    char *p;
    struct comuser *user = select_user(ch, player);
    raw_notify(player, tprintf("You have left channel %s.", ch->name));
    if ((user->bUserIsOn) && (!Dark(player)))
    {
        if (user->title[0] != '\0')
        {
            // There is a comtitle
            //
            if (ch->type & CHANNEL_SPOOF)
            {
                p = tprintf( "%s %s has left this channel.",
                             ch->header, user->title);
            }
            else
            {
                p = tprintf( "%s %s %s has left this channel.",
                             ch->header, user->title, Name(player));
            }
        }
        else
        {
            p = tprintf( "%s %s has left this channel.", ch->header,
                         Name(player));
        }
        do_comsend(ch, p, player);
    }
    user->bUserIsOn = 0;
}

void do_comwho_line
(
    dbref player,
    struct channel *ch,
    struct comuser *user
)
{
    char *msg;
    char *buff = NULL;

    if (user->title[0] != '\0')
    {
        // There is a comtitle
        //
        if (Staff(player))
        {
            buff = unparse_object(player, user->who, 0);
            if (ch->type & CHANNEL_SPOOF)
            {
                msg = tprintf("%s as %s", buff, user->title);
            }
            else
            {
                msg = tprintf("%s as %s %s", buff, user->title, buff);
            }
        }
        else
        {
            if (ch->type & CHANNEL_SPOOF)
            {
                msg = user->title;
            }
            else
            {
                buff = unparse_object(player, user->who, 0);
                msg = tprintf("%s %s", user->title, buff);
            }
        }
    }
    else
    {
        buff = unparse_object(player, user->who, 0);
        msg = buff;
    }

    raw_notify(player, msg);
    if (buff)
    {
        free_lbuf(buff);
    }
}

void do_comwho(dbref player, struct channel *ch)
{
    struct comuser *user;

    raw_notify(player, "-- Players --");
    for (user = ch->on_users; user; user = user->on_next)
    {
        if (isPlayer(user->who))
        {
            if (Connected(user->who) && (!Dark(user->who) || Wizard_Who(player)))
            {
                if (user->bUserIsOn)
                {
                    do_comwho_line(player, ch, user);
                }
            }
            else if (!Dark(user->who))
            {
                do_comdisconnectchannel(user->who, ch->name);
            }
        }
    }
    raw_notify(player, "-- Objects --");
    for (user = ch->on_users; user; user = user->on_next)
    {
        if (!isPlayer(user->who))
        {
            if ((Going(user->who)) && (God(Owner(user->who))))
            {
                do_comdisconnectchannel(user->who, ch->name);
            }
            else if (user->bUserIsOn)
            {
                do_comwho_line(player, ch, user);
            }
        }
    }
    raw_notify(player, tprintf("-- %s --", ch->name));
}

struct channel *select_channel(char *channel)
{
    struct channel *cp;

    cp = (struct channel *)hashfindLEN(channel, strlen(channel), &mudstate.channel_htab);
    if (!cp)
        return NULL;
    else
        return cp;
}

struct comuser *select_user(struct channel *ch, dbref player)
{
    int first, last, current;
    int dir;

    if (!ch)
        return NULL;

    first = 0;
    last = ch->num_users - 1;
    dir = 1;
    current = (first + last) / 2;

    while (dir && (first <= last))
    {
        current = (first + last) / 2;
        if (ch->users[current] == NULL)
        {
            last--;
            continue;
        }
        if (ch->users[current]->who == player)
            dir = 0;
        else if (ch->users[current]->who < player)
        {
            dir = 1;
            first = current + 1;
        }
        else
        {
            dir = -1;
            last = current - 1;
        }
    }

    if (!dir)
        return ch->users[current];
    else
        return NULL;
}

#define MAX_ALIASES_PER_PLAYER 50

void do_addcom(dbref player, dbref cause, int key, char *arg1, char *arg2)
{
    char channel[MAX_CHANNEL_LEN+1];
    char title_tmp[LBUF_SIZE];
    char Buffer[MAX_CHANNEL_LEN+1];
    struct channel *ch;
    char *s, *t;
    int i, j, where;
    comsys_t *c;
    char *na;
    char **nc;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    BOOL bValidAlias;
    int  nValidAlias;
    char *pValidAlias = MakeCanonicalComAlias(arg1, &nValidAlias, &bValidAlias);
    if (!bValidAlias)
    {
        raw_notify(player, "You need to specify a valid alias.");
        return;
    }
    s = arg2;
    if (!*s)
    {
        raw_notify(player, "You need to specify a channel.");
        return;
    }
    t = channel;
    while (*s && (*s != ',') && ((t - channel) < MAX_CHANNEL_LEN))
    {
        if (*s != ' ')
            *t++ = *s++;
        else
            s++;
    }
    *t = '\0';

    t = title_tmp;
    *t = '\0';
    if (*s)
    {
        // Read title
        //
        s++;
        int n = sizeof(title_tmp) - 1;
        while (*s && t - title_tmp < n)
        {
            *t++ = *s++;
        }
        *t = '\0';
    }
    ch = select_channel(channel);
    if (!ch)
    {
        int nVisualWidth;
        ANSI_TruncateToField(channel, sizeof(Buffer), Buffer, sizeof(Buffer), &nVisualWidth, ANSI_ENDGOAL_NORMAL);
        raw_notify(player, tprintf("Channel %s does not exist yet.", Buffer));
        return;
    }
    if (!(do_test_access(player, CHANNEL_JOIN, ch)))
    {
        raw_notify(player, "Sorry, this channel type does not allow you to join.");
        return;
    }
    if (select_user(ch, player))
    {
        raw_notify(player, tprintf("Warning: You are already on that channel."));
    }
    c = get_comsys(player);
    if (c->numchannels >= MAX_ALIASES_PER_PLAYER)
    {
        raw_notify(player, tprintf("Sorry, but you have reached the maximum number of aliases allowed."));
        return;
    }
    for (j = 0; j < c->numchannels && (strcmp(pValidAlias, c->alias + j * 6) > 0); j++)
    {
        // Nothing
        ;
    }
    if (j < c->numchannels && !strcmp(pValidAlias, c->alias + j * 6))
    {
        char *p = tprintf("That alias is already in use for channel %s.", c->channels[j]);
        raw_notify(player, p);
        return;
    }
    if (c->numchannels >= c->maxchannels)
    {
        c->maxchannels += 10;

        na = (char *)MEMALLOC(6 * c->maxchannels);
        (void)ISOUTOFMEMORY(na);
        nc = (char **)MEMALLOC(sizeof(char *) * c->maxchannels);
        (void)ISOUTOFMEMORY(nc);

        for (i = 0; i < c->numchannels; i++)
        {
            strcpy(na + i * 6, c->alias + i * 6);
            nc[i] = c->channels[i];
        }
        if (c->alias)
        {
            MEMFREE(c->alias);
        }
        if (c->channels)
        {
            MEMFREE(c->channels);
        }
        c->alias = na;
        c->channels = nc;
    }
    where = c->numchannels++;
    for (i = where; i > j; i--)
    {
        strcpy(c->alias + i * 6, c->alias + (i - 1) * 6);
        c->channels[i] = c->channels[i - 1];
    }

    where = j;
    memcpy(c->alias + where * 6, pValidAlias, nValidAlias);
    *(c->alias + where * 6 + nValidAlias) = '\0';
    c->channels[where] = StringClone(channel);

    do_joinchannel(player, ch);
    char *pValidatedTitleValue = RestrictTitleValue(title_tmp);
    do_setnewtitle(player, ch, pValidatedTitleValue);

    if (pValidatedTitleValue[0] == '\0')
    {
        raw_notify(player, tprintf("Channel %s added with alias %s.", channel, pValidAlias));
    }
    else
    {
        raw_notify(player, tprintf("Channel %s added with alias %s and title %s.", channel, pValidAlias, pValidatedTitleValue));
    }
}

void do_delcom(dbref player, dbref cause, int key, char *arg1)
{
    int i;
    comsys_t *c;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    if (!arg1)
    {
        raw_notify(player, "Need an alias to delete.");
        return;
    }
    c = get_comsys(player);

    for (i = 0; i < c->numchannels; i++)
    {
        if (!strcmp(arg1, c->alias + i * 6))
        {
            do_delcomchannel(player, c->channels[i]);
            raw_notify(player, tprintf("Channel %s deleted.", c->channels[i]));
            MEMFREE(c->channels[i]);

            c->numchannels--;
            for (; i < c->numchannels; i++)
            {
                strcpy(c->alias + i * 6, c->alias + i * 6 + 6);
                c->channels[i] = c->channels[i + 1];
            }
            return;
        }
    }
    raw_notify(player, "Unable to find that alias.");
}

void do_delcomchannel(dbref player, char *channel)
{
    struct comuser *user;
    int i;
    int j;

    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(player, tprintf("Unknown channel %s.", channel));
    }
    else
    {
        j = 0;
        for (i = 0; i < ch->num_users && !j; i++)
        {
            user = ch->users[i];
            if (user->who == player)
            {
                do_comdisconnectchannel(player, channel);
                if (user->bUserIsOn && (!Dark(player)))
                {
                    char *p;
                    if (user->title[0] != '\0')
                    {
                        // There is a comtitle.
                        //
                        if (ch->type & CHANNEL_SPOOF)
                        {
                            p = tprintf("%s %s has left this channel.", ch->header, user->title);
                        }
                        else
                        {
                            p = tprintf("%s %s %s has left this channel.", ch->header, user->title, Name(player));
                        }
                    }
                    else
                    {
                        p = tprintf("%s %s has left this channel.", ch->header, Name(player));
                    }
                    do_comsend(ch, p, player);
                }
                raw_notify(player, tprintf("You have left channel %s.", channel));

                if (user->title)
                {
                    MEMFREE(user->title);
                }
                MEMFREE(user);
                j = 1;
            }
        }

        if (j)
        {
            ch->num_users--;
            for (i--; i < ch->num_users; i++)
            {
                ch->users[i] = ch->users[i + 1];
            }
        }
    }
}

void do_createchannel(dbref player, dbref cause, int key, char *channel)
{
    struct channel *newchannel;

    if (select_channel(channel))
    {
        raw_notify(player, tprintf("Channel %s already exists.", channel));
        return;
    }
    if (!*channel)
    {
        raw_notify(player, "You must specify a channel to create.");
        return;
    }
    if (!(Comm_All(player)))
    {
        raw_notify(player, "You do not have permission to do that.");
        return;
    }
    newchannel = (struct channel *)MEMALLOC(sizeof(struct channel));
    (void)ISOUTOFMEMORY(newchannel);

    int   vwChannel;
    unsigned int nNameNoANSI;
    char *pNameNoANSI;
    char Buffer[MAX_HEADER_LEN];
    int nChannel = ANSI_TruncateToField(channel, sizeof(Buffer),
        Buffer, sizeof(Buffer), &vwChannel, ANSI_ENDGOAL_NORMAL);
    if (nChannel == vwChannel)
    {
        // The channel name does not contain ANSI, so first, we add some to
        // get the header.
        //
        const int nMax = MAX_HEADER_LEN - (sizeof(ANSI_HILITE)-1)
                       - (sizeof(ANSI_NORMAL)-1) - 2;
        if (nChannel > nMax)
        {
            nChannel = nMax;
        }
        Buffer[nChannel] = '\0';
        sprintf(newchannel->header, "%s[%s]%s", ANSI_HILITE, Buffer,
            ANSI_NORMAL);

        // Then, we use the non-ANSI part for the name.
        //
        nNameNoANSI = nChannel;
        pNameNoANSI = Buffer;
    }
    else
    {
        // The given channel name does contain ANSI.
        //
        memcpy(newchannel->header, Buffer, nChannel+1);
        pNameNoANSI = strip_ansi(Buffer, &nNameNoANSI);
    }
    if (nNameNoANSI > MAX_CHANNEL_LEN)
    {
        nNameNoANSI = MAX_CHANNEL_LEN;
    }
    memcpy(newchannel->name, pNameNoANSI, nNameNoANSI);
    newchannel->name[nNameNoANSI] = '\0';

    newchannel->type = 127;
    newchannel->temp1 = 0;
    newchannel->temp2 = 0;
    newchannel->charge = 0;
    newchannel->charge_who = player;
    newchannel->amount_col = 0;
    newchannel->num_users = 0;
    newchannel->max_users = 0;
    newchannel->users = NULL;
    newchannel->on_users = NULL;
    newchannel->chan_obj = NOTHING;
    newchannel->num_messages = 0;

    num_channels++;

    hashaddLEN(newchannel->name, strlen(newchannel->name), (int *)newchannel, &mudstate.channel_htab);

    // Report the channel creation using non-ANSI name.
    //
    raw_notify(player, tprintf("Channel %s created.", newchannel->name));
}

void do_destroychannel(dbref player, dbref cause, int key, char *channel)
{
    struct channel *ch;
    int j;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    ch = (struct channel *)hashfindLEN(channel, strlen(channel), &mudstate.channel_htab);

    if (!ch)
    {
        raw_notify(player, tprintf("Could not find channel %s.", channel));
        return;
    }
    else if (!(Comm_All(player)) && (player != ch->charge_who))
    {
        raw_notify(player, "You do not have permission to do that. ");
        return;
    }
    num_channels--;
    hashdeleteLEN(channel, strlen(channel), &mudstate.channel_htab);

    for (j = 0; j < ch->num_users; j++)
    {
        MEMFREE(ch->users[j]);
    }
    MEMFREE(ch->users);
    MEMFREE(ch);
    raw_notify(player, tprintf("Channel %s destroyed.", channel));
}

#if 0
void do_cleanupchannels(void)
{
    struct channel *ch;
    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        struct comuser *user, *prevuser = NULL;
        for (user = ch->on_users; user; )
        {
            if (isPlayer(user->who))
            {
                if (!(do_test_access(user->who, CHANNEL_JOIN, ch)))
                //if (!Connected(user->who))
                {
                    // Go looking for user in the array.
                    //
                    int bFound = FALSE;
                    int iPos;
                    for (iPos = 0; iPos < ch->num_users && !bFound; iPos++)
                    {
                        if (ch->users[iPos] == user)
                        {
                            bFound = TRUE;
                        }
                    }

                    if (bFound)
                    {
                        // Remove user from the array.
                        //
                        ch->num_users--;
                        for (iPos--; iPos < ch->num_users; iPos++)
                        {
                            ch->users[iPos] = ch->users[iPos+1];
                        }

                        // Save user pointer for later reporting and freeing.
                        //
                        struct comuser *cuVictim = user;

                        // Unlink user from the list, and decide who to look at next.
                        //
                        if (prevuser)
                        {
                            prevuser->on_next = user->on_next;
                        }
                        else
                        {
                            ch->on_users = user->on_next;
                        }
                        user = user->on_next;

                        // Reporting
                        //
                        if (!Dark(cuVictim->who))
                        {
                            char buff[LBUF_SIZE];
                            if (cuVictim->title[0] != '\0')
                            {
                                // There is a comtitle.
                                //
                                if (ch->type & CHANNEL_SPOOF)
                                {
                                    sprintf(buff, "[%s] The system boots %s off the channel.", ch->name, cuVictim->title);
                                }
                                else
                                {
                                    sprintf(buff, "[%s] The system boots %s %s off the channel.", ch->name, cuVictim->title, Name(cuVictim->who));
                                }
                            }
                            else
                            {
                                sprintf(buff, "[%s] The system boots %s off the channel.", ch->name, Name(cuVictim->who));
                            }
                            do_comsend(ch, buff, player);
                        }
                        raw_notify(cuVictim->who, tprintf("The system has booted you off channel %s.", ch->name));

                        // Freeing
                        //
                        if (cuVictim->title)
                        {
                            MEMFREE(cuVictim->title);
                        }
                        MEMFREE(cuVictim);

                        continue;
                    }
                }
            }

            prevuser = user;
            user = user->on_next;
        }
    }
}
#endif

void do_listchannels(dbref player)
{
    struct channel *ch;
    char temp[LBUF_SIZE];

    int perm = Comm_All(player);
    if (!perm)
    {
        raw_notify(player, "Warning: Only public channels and your channels will be shown.");
    }
    raw_notify(player, "*** Channel      --Flags--  Obj   Own   Charge  Balance  Users   Messages");

    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (perm || (ch->type & CHANNEL_PUBLIC) || ch->charge_who == player)
        {

            sprintf(temp, "%c%c%c %-13.13s %c%c%c/%c%c%c %5d %5d %8d %8d %6d %10d",
                (ch->type & (CHANNEL_PUBLIC)) ? 'P' : '-',
                (ch->type & (CHANNEL_LOUD)) ? 'L' : '-',
                (ch->type & (CHANNEL_SPOOF)) ? 'S' : '-',
                ch->name,
                (ch->type & (CHANNEL_PL_MULT * CHANNEL_JOIN)) ? 'J' : '-',
                (ch->type & (CHANNEL_PL_MULT * CHANNEL_TRANSMIT)) ? 'X' : '-',
                (ch->type & (CHANNEL_PL_MULT * CHANNEL_RECEIVE)) ? 'R' : '-',
                (ch->type & (CHANNEL_OBJ_MULT * CHANNEL_JOIN)) ? 'j' : '-',
                (ch->type & (CHANNEL_OBJ_MULT * CHANNEL_TRANSMIT)) ? 'x' : '-',
                (ch->type & (CHANNEL_OBJ_MULT * CHANNEL_RECEIVE)) ? 'r' : '-',
                (ch->chan_obj != NOTHING) ? ch->chan_obj : -1,
                ch->charge_who, ch->charge, ch->amount_col, ch->num_users, ch->num_messages);
            raw_notify(player, temp);
        }
    }
    raw_notify(player, "-- End of list of Channels --");
}

void do_comtitle(dbref player, dbref cause, int key, char *arg1, char *arg2)
{
    char channel[MAX_CHANNEL_LEN+1];

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    if (!*arg1)
    {
        raw_notify(player, "Need an alias to do comtitle.");
        return;
    }
    strcpy(channel, get_channel_from_alias(player, arg1));

    if (channel[0] == '\0')
    {
        raw_notify(player, "Unknown alias");
        return;
    }
    struct channel *ch = select_channel(channel);
    if (ch)
    {
        if (select_user(ch, player))
        {
            char *pValidatedTitleValue = RestrictTitleValue(arg2);
            do_setnewtitle(player, ch, pValidatedTitleValue);
            raw_notify(player, tprintf("Title set to '%s' on channel %s.",
                       pValidatedTitleValue, channel));
        }
    }
    else
    {
        raw_notify(player, "Illegal comsys alias, please delete.");
    }
}

void do_comlist(dbref player, dbref cause, int key)
{
    comsys_t *c;
    int i;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    c = get_comsys(player);

    raw_notify(player, "Alias     Channel            Status Title");

    for (i = 0; i < c->numchannels; i++)
    {
        struct comuser *user = select_user(select_channel(c->channels[i]), player);
        if (user)
        {
            char *p = tprintf("%-9.9s %-18.18s %-6.6s %s", c->alias + i * 6, c->channels[i], (user->bUserIsOn ? "on" : "off"), user->title);
            raw_notify(player, p);
        }
        else
        {
            raw_notify(player, tprintf("Bad Comsys Alias: %s for Channel: %s", c->alias + i * 6, c->channels[i]));
        }
    }
    raw_notify(player, "-- End of comlist --");
}

void do_channelnuke(dbref player)
{
    struct channel *ch;
    int j;

    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (ch->charge_who == player)
        {
            num_channels--;
            hashdeleteLEN(ch->name, strlen(ch->name), &mudstate.channel_htab);

            for (j = 0; j < ch->num_users; j++)
            {
                MEMFREE(ch->users[j]);
            }
            MEMFREE(ch->users);
            MEMFREE(ch);
        }
    }
}

void do_clearcom(dbref player, dbref unused1, int unused2)
{
    int i;
    comsys_t *c;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    c = get_comsys(player);

    for (i = (c->numchannels) - 1; i > -1; --i)
    {
        do_delcom(player, player, 0, c->alias + i * 6);
    }
}

void do_allcom(dbref player, dbref cause, int key, char *arg1)
{
    int i;
    comsys_t *c;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    c = get_comsys(player);

    if (  strcmp(arg1, "who") != 0
       && strcmp(arg1, "on")  != 0
       && strcmp(arg1, "off") != 0)
    {
        raw_notify(player, "Only options available are: on, off and who.");
        return;
    }
    for (i = 0; i < c->numchannels; i++)
    {
        do_processcom(player, c->channels[i], arg1);
        if (strcmp(arg1, "who") == 0)
        {
            raw_notify(player, "");
        }
    }
}

void sort_users(struct channel *ch)
{
    int i;
    int nu;
    int done;
    struct comuser *user;

    nu = ch->num_users;
    done = 0;
    while (!done)
    {
        done = 1;
        for (i = 0; i < (nu - 1); i++)
        {
            if (ch->users[i]->who > ch->users[i + 1]->who)
            {
                user = ch->users[i];
                ch->users[i] = ch->users[i + 1];
                ch->users[i + 1] = user;
                done = 0;
            }
        }
    }
}

void do_channelwho(dbref player, dbref cause, int key, char *arg1)
{
    struct comuser *user;
    char channel[MAX_CHANNEL_LEN+1];
    int flag;
    char *s;
    char *t;
    char *buff;
    char temp[LBUF_SIZE];
    int i;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    s = arg1;
    t = channel;
    while (*s && *s != '/' && ((t - channel) < MAX_CHANNEL_LEN))
    {
        *t++ = *s++;
    }
    *t = 0;

    flag = 0;
    if (*s && *(s + 1))
        flag = (*(s + 1) == 'a');

    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(player, tprintf("Unknown channel %s.", channel));
        return;
    }
    if (!((Comm_All(player)) || (player == ch->charge_who)))
    {
        raw_notify(player, "You do not have permission to do that. (Not owner or admin.)");
        return;
    }
    raw_notify(player, tprintf("-- %s --", ch->name));
    raw_notify(player, tprintf("%-29.29s %-6.6s %-6.6s", "Name", "Status", "Player"));
    for (i = 0; i < ch->num_users; i++)
    {
        user = ch->users[i];
        if ((flag || UNDEAD(user->who)) && (!Dark(user->who) || Wizard_Who(player)))
        {
            buff = unparse_object(player, user->who, 0);
            sprintf(temp, "%-29.29s %-6.6s %-6.6s", strip_ansi(buff),
                user->bUserIsOn ? "on " : "off",
                isPlayer(user->who) ? "yes" : "no ");
            raw_notify(player, temp);
            free_lbuf(buff);
        }
    }
    raw_notify(player, tprintf("-- %s --", ch->name));
}

void do_comdisconnectraw_notify(dbref player, char *chan)
{
    struct channel *ch = select_channel(chan);
    if (!ch) return;

    struct comuser *cu = select_user(ch, player);
    if (!cu) return;

    if ((ch->type & CHANNEL_LOUD) && (cu->bUserIsOn) && (!Dark(player)))
    {
        char *buff = alloc_lbuf("do_comconnect");
        if (cu->title[0] != '\0')
        {
            // There is a comtitle.
            //
            if (ch->type & CHANNEL_SPOOF)
            {
                sprintf(buff, "%s %s has disconnected.", ch->header, cu->title);
            }
            else
            {
                sprintf(buff, "%s %s %s has disconnected.", ch->header, cu->title, Name(player));
            }
        }
        else
        {
            sprintf(buff, "%s %s has disconnected.", ch->header, Name(player));
        }
        do_comsend(ch, buff, player);
        free_lbuf(buff);
    }
}

void do_comconnectraw_notify(dbref player, char *chan)
{
    char *buff;

    struct channel *ch = select_channel(chan);
    if (!ch) return;
    struct comuser *cu = select_user(ch, player);
    if (!cu) return;

    if ((ch->type & CHANNEL_LOUD) && (cu->bUserIsOn) && (!Dark(player)))
    {
        buff = alloc_lbuf("do_comconnect");
        if (cu->title[0] != '\0')
        {
            // There is a comtitle.
            //
            if (ch->type & CHANNEL_SPOOF)
            {
                sprintf(buff, "%s %s has connected.", ch->header, cu->title);
            }
            else
            {
                sprintf(buff, "%s %s %s has connected.", ch->header, cu->title, Name(player));
            }
        }
        else
        {
            sprintf(buff, "%s %s has connected.", ch->header, Name(player));
        }
        do_comsend(ch, buff, player);
        free_lbuf(buff);
    }
}

void do_comconnectchannel(dbref player, char *channel, char *alias, int i)
{
    struct comuser *user;

    struct channel *ch = select_channel(channel);
    if (ch)
    {
        for (user = ch->on_users;
        user && user->who != player;
        user = user->on_next) ;

        if (!user)
        {
            user = select_user(ch, player);
            if (user)
            {
                user->on_next = ch->on_users;
                ch->on_users = user;
            }
            else
            {
                raw_notify(player, tprintf("Bad Comsys Alias: %s for Channel: %s", alias + i * 6, channel));
            }
        }
    }
    else
    {
        raw_notify(player, tprintf("Bad Comsys Alias: %s for Channel: %s", alias + i * 6, channel));
    }
}

void do_comdisconnect(dbref player)
{
    int i;
    comsys_t *c;

    c = get_comsys(player);

    for (i = 0; i < c->numchannels; i++)
    {
        do_comdisconnectchannel(player, c->channels[i]);
#ifdef CHANNEL_LOUD
        do_comdisconnectraw_notify(player, c->channels[i]);
#endif // CHANNEL_LOUD
    }
}

void do_comconnect(dbref player)
{
    comsys_t *c;
    int i;

    c = get_comsys(player);

    for (i = 0; i < c->numchannels; i++)
    {
        do_comconnectchannel(player, c->channels[i], c->alias, i);
        do_comconnectraw_notify(player, c->channels[i]);
    }
}


void do_comdisconnectchannel(dbref player, char *channel)
{
    struct comuser *prevuser = NULL;
    struct channel *ch = select_channel(channel);
    if (!ch)
        return;

    struct comuser *user;
    for (user = ch->on_users; user;)
    {
        if (user->who == player)
        {
            if (prevuser)
                prevuser->on_next = user->on_next;
            else
                ch->on_users = user->on_next;
            return;
        }
        else
        {
            prevuser = user;
            user = user->on_next;
        }
    }
}

void do_editchannel(dbref player, dbref cause, int flag, char *arg1, char *arg2)
{
    char *s;
    int add_remove = 1;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    struct channel *ch = select_channel(arg1);
    if (!ch)
    {
        raw_notify(player, tprintf("Unknown channel %s.", arg1));
        return;
    }
    if (!((Comm_All(player)) || (player == ch->charge_who)))
    {
        raw_notify(player, "You do not have permission to do that. (Not owner or Admin.)");
        return;
    }
    s = arg2;
    if (*s == '!')
    {
        add_remove = 0;
        s++;
    }
    switch (flag)
    {
    case 0:
        {
            int who = lookup_player(player, arg2, 1);
            if (NOTHING == who)
            {
                raw_notify(player, "Invalid player.");
            }
            else
            {
                ch->charge_who = who;
                raw_notify(player, "Set.");
            }
        }
        break;

    case 1:
        ch->charge = Tiny_atol(arg2);
        raw_notify(player, "Set.");
        break;

    case 3:
        if (strcmp(s, "join") == 0)
        {
            if (add_remove)
            {
                ch->type |= (CHANNEL_PL_MULT * CHANNEL_JOIN);
                raw_notify(player, "@cpflags: Set.");
            }
            else
            {
                ch->type &= ~(CHANNEL_PL_MULT * CHANNEL_JOIN);
                raw_notify(player, "@cpflags: Cleared.");
            }
        }
        else if (strcmp(s, "receive") == 0)
        {
            if (add_remove)
            {
                ch->type |= (CHANNEL_PL_MULT * CHANNEL_RECEIVE);
                raw_notify(player, "@cpflags: Set.");
            }
            else
            {
                ch->type &= ~(CHANNEL_PL_MULT * CHANNEL_RECEIVE);
                raw_notify(player, "@cpflags: Cleared.");
            }
        }
        else if (strcmp(s, "transmit") == 0)
        {
            if (add_remove)
            {
                ch->type |= (CHANNEL_PL_MULT * CHANNEL_TRANSMIT);
                raw_notify(player, "@cpflags: Set.");
            }
            else
            {
                ch->type &= ~(CHANNEL_PL_MULT * CHANNEL_TRANSMIT);
                raw_notify(player, "@cpflags: Cleared.");
            }
        }
        else
        {
            raw_notify(player, "@cpflags: Unknown Flag.");
        }
        break;

    case 4:
        if (strcmp(s, "join") == 0)
        {
            if (add_remove)
            {
                ch->type |= (CHANNEL_OBJ_MULT * CHANNEL_JOIN);
                raw_notify(player, "@coflags: Set.");
            }
            else
            {
                ch->type &= ~(CHANNEL_OBJ_MULT * CHANNEL_JOIN);
                raw_notify(player, "@coflags: Cleared.");
            }
        }
        else if (strcmp(s, "receive") == 0)
        {
            if (add_remove)
            {
                ch->type |= (CHANNEL_OBJ_MULT * CHANNEL_RECEIVE);
                raw_notify(player, "@coflags: Set.");
            }
            else
            {
                ch->type &= ~(CHANNEL_OBJ_MULT * CHANNEL_RECEIVE);
                raw_notify(player, "@coflags: Cleared.");
            }
        }
        else if (strcmp(s, "transmit") == 0)
        {
            if (add_remove)
            {
                ch->type |= (CHANNEL_OBJ_MULT * CHANNEL_TRANSMIT);
                raw_notify(player, "@coflags: Set.");
            }
            else
            {
                ch->type &= ~(CHANNEL_OBJ_MULT * CHANNEL_TRANSMIT);
                raw_notify(player, "@coflags: Cleared.");
            }
        }
        else
        {
            raw_notify(player, "@coflags: Unknown Flag.");
        }
        break;
    }
}

int do_test_access(dbref player, long access, struct channel *chan)
{
    long flag_value = access;

    if (Comm_All(player))
    {
        return 1;
    }

    // Channel objects allow custom locks for channels.  The normal
    // lock is used to see if they can join that channel. The enterlock
    // is checked to see if they can receive messages on it. The
    // Uselock is checked to see if they can transmit on it. Note:
    // These checks do not supercede the normal channel flags. If a
    // channel is set JOIN for players, ALL players can join the
    // channel, whether or not they pass the lock.  Same for all
    // channel object locks.
    //
    if (chan->chan_obj != NOTHING && chan->chan_obj != 0)
    {
        if (flag_value & CHANNEL_JOIN)
        {
            if (could_doit(player, chan->chan_obj, A_LOCK))
                return 1;
        }
        if (flag_value & CHANNEL_TRANSMIT)
        {
            if (could_doit(player, chan->chan_obj, A_LUSE))
                return 1;
        }
        if (flag_value & CHANNEL_RECEIVE)
        {
            if (could_doit(player, chan->chan_obj, A_LENTER))
                return 1;
        }
    }

    if (isPlayer(player))
    {
        flag_value *= CHANNEL_PL_MULT;
    }
    else
    {
        flag_value *= CHANNEL_OBJ_MULT;
    }

    // Mask out CHANNEL_PUBLIC, CHANNEL_LOUD, and CHANNEL_SPOOF
    //
    flag_value &= 0xFF;

    return (((long)chan->type & flag_value));
}

// 1 means continue, 0 means stop
//
int do_comsystem(dbref who, char *cmd)
{
    char *t;
    char *alias = alloc_lbuf("do_comsystem");
    char *s = alias;
    for (t = cmd; *t && *t != ' '; *s++ = *t++)
    {
        ; // Nothing.
    }

    *s = '\0';

    if (*t)
    {
        t++;
    }

    char *ch = get_channel_from_alias(who, alias);
    if (ch[0] != '\0')
    {
        do_processcom(who, ch, t);
        free_lbuf(alias);
        return 0;
    }
    else
    {
        free_lbuf(alias);
    }
    return 1;
}

void do_cemit(dbref player, dbref cause, int key, char *chan, char *text)
{
    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    struct channel *ch = select_channel(chan);
    if (!ch)
    {
        raw_notify(player, tprintf("Channel %s does not exist.", chan));
        return;
    }
    if ((player != ch->charge_who) && (!Comm_All(player)))
    {
        raw_notify(player, NOPERM_MESSAGE);
        return;
    }
    if (key == CEMIT_NOHEADER)
    {
        do_comsend(ch, text, player);
    }
    else
    {
        do_comsend(ch, tprintf("%s %s", ch->header, text), player);
    }
}

void do_chopen(dbref player, dbref cause, int key, char *chan, char *value)
{
    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    if (key == CSET_LIST)
    {
        do_chanlist(player, NOTHING, 1);
        return;
    }

    char *msg = NULL;
    char *buff;
    dbref thing;

    struct channel *ch = select_channel(chan);
    if (!ch)
    {
        msg = tprintf("@cset: Channel %s does not exist.", chan);
        raw_notify(player, msg);
        return;
    }
    if ((player != ch->charge_who) && (!Comm_All(player)))
    {
        raw_notify(player, NOPERM_MESSAGE);
        return;
    }
    switch (key)
    {
    case CSET_PUBLIC:
        ch->type |= CHANNEL_PUBLIC;
        msg = tprintf("@cset: Channel %s placed on the public listings.", chan);
        break;

    case CSET_PRIVATE:
        ch->type &= ~CHANNEL_PUBLIC;
        msg = tprintf("@cset: Channel %s taken off the public listings." ,chan);
        break;

    case CSET_LOUD:
        ch->type |= CHANNEL_LOUD;
        msg = tprintf("@cset: Channel %s now sends connect/disconnect msgs.", chan);
        break;

    case CSET_QUIET:
        ch->type &= ~CHANNEL_LOUD;
        msg = tprintf("@cset: Channel %s connect/disconnect msgs muted.", chan);
        break;

    case CSET_SPOOF:
        ch->type |= CHANNEL_SPOOF;
        msg = tprintf("@cset: Channel %s set spoofable.", chan);
        break;

    case CSET_NOSPOOF:
        ch->type &= ~CHANNEL_SPOOF;
        msg = tprintf("@cset: Channel %s set unspoofable.", chan);
        break;

    case CSET_OBJECT:
        init_match(player, value, NOTYPE);
        match_everything(0);
        thing = match_result();

        ch->chan_obj = thing;
        if (thing == NOTHING)
        {
            msg = tprintf("Channel %s is now disassociated from any channel object.", ch->name);
        }
        else
        {
            buff = unparse_object(player, thing, 0);
            msg = tprintf("Channel %s is now using %s as channel object.", ch->name, buff);
            free_lbuf(buff);
        }
        break;

    case CSET_HEADER:
        do_cheader(player, chan, value);
        msg = "Set.";
        break;
    }
    raw_notify(player, msg);
}

void do_chboot(dbref player, dbref cause, int key, char *channel, char *victim)
{
    dbref thing;
    char buff[LBUF_SIZE];
    char buf2[LBUF_SIZE];

    // I sure hope it's not going to be that long.
    //
    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(player, "@cboot: Unknown channel.");
        return;
    }
    struct comuser *user = select_user(ch, player);
    if (!user)
    {
        raw_notify(player, "@cboot: You are not on that channel.");
        return;
    }
    if (!(ch->charge_who == player) && !Comm_All(player))
    {
        raw_notify(player, "@cboot:  You can't do that!");
        return;
    }
    thing = match_thing(player, victim);

    if (thing == NOTHING)
    {
        return;
    }
    struct comuser *vu = select_user(ch, thing);
    if (!vu)
    {
        raw_notify(player, tprintf("@cboot: %s is not on the channel.", Name(thing)));
        return;
    }

    // We should be in the clear now. ;)
    //
    if (user->title[0] != '\0')
    {
        // There is a comtitle.
        //
        if (ch->type & CHANNEL_SPOOF)
        {
            strcpy(buf2, user->title);
        }
        else
        {
            sprintf(buf2, "%s %s", user->title, Name(player));
        }
    }
    else
    {
        strcpy(buf2, Name(player));
    }
    if (vu->title[0] != '\0')
    {
        // There is a comtitle.
        //
        if (ch->type & CHANNEL_SPOOF)
        {
            sprintf(buff, "%s %s boots %s off the channel.", ch->header, buf2, vu->title);
        }
        else
        {
            sprintf(buff, "%s %s boots %s %s off the channel.", ch->header, buf2, vu->title, Name(thing));
        }
    }
    else
    {
        sprintf(buff, "%s %s boots %s off the channel.", ch->header, buf2, Name(thing));
    }
    do_comsend(ch, buff, player);
    do_delcomchannel(thing, channel);
}

void do_cheader(dbref player, char *channel, char *header)
{
    struct channel *ch = select_channel(channel);
    if (!ch)
    {
        raw_notify(player, "That channel does not exist.");
        return;
    }
    if (!(ch->charge_who == player) && !Comm_All(player))
    {
        raw_notify(player, "Permission denied.");
        return;
    }
    char *p = RemoveSetOfCharacters(header, "\r\n\t");

    // Optimize/terminate any ANSI in the string.
    //
    char NewHeader_ANSI[MAX_HEADER_LEN+1];
    int nVisualWidth;
    int nLen = ANSI_TruncateToField(p, sizeof(NewHeader_ANSI),
        NewHeader_ANSI, sizeof(NewHeader_ANSI), &nVisualWidth,
        ANSI_ENDGOAL_NORMAL);
    memcpy(ch->header, NewHeader_ANSI, nLen+1);
}


void do_chanlist(dbref player, dbref cause, int key)
{
    dbref owner;
    struct channel *ch;
    int flags;
    char *temp;
    char *buf;
    char *atrstr;

    if (!mudconf.have_comsys)
    {
        raw_notify(player, "Comsys disabled.");
        return;
    }
    flags = 0;

    if (key & CLIST_FULL)
    {
        do_listchannels(player);
        return;
    }
    temp = alloc_mbuf("do_chanlist_temp");
    buf = alloc_mbuf("do_chanlist_buf");

    if (key & CLIST_HEADERS)
    {
        raw_notify(player, "*** Channel       Owner           Header");
    }
    else
    {
        raw_notify(player, "*** Channel       Owner           Description");
    }

    for (ch = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         ch; ch = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (Comm_All(player) || (ch->type & CHANNEL_PUBLIC) ||
            ch->charge_who == player)
        {
            char *pBuffer;
            if (key & CLIST_HEADERS)
            {
                pBuffer = ch->header;
            }
            else
            {
                atrstr = atr_pget(ch->chan_obj, A_DESC, &owner, &flags);
                if ((ch->chan_obj == NOTHING) || !*atrstr)
                    strcpy(buf, "No description.");
                else
                    sprintf(buf, "%-54.54s", atrstr);
                free_lbuf(atrstr);

                pBuffer = buf;
            }
            sprintf(temp, "%c%c%c %-13.13s %-15.15s %-45.45s",
                (ch->type & (CHANNEL_PUBLIC)) ? 'P' : '-',
                (ch->type & (CHANNEL_LOUD)) ? 'L' : '-',
                (ch->type & (CHANNEL_SPOOF)) ? 'S' : '-',
                ch->name, Name(ch->charge_who), pBuffer);

            raw_notify(player, temp);
        }
    }
    free_mbuf(temp);
    free_mbuf(buf);
    raw_notify(player, "-- End of list of Channels --");
}

// Returns a player's comtitle for a named channel.
//
FUNCTION(fun_comtitle)
{
    if (!mudconf.have_comsys)
    {
        safe_str("#-1 COMSYS DISABLED", buff, bufc);
        return;
    }

    dbref victim = lookup_player(player, fargs[0], 1);
    comsys_t *c = get_comsys(player);

    if (victim == NOTHING)
    {
        safe_str("#-1 PLAYER DOES NOT EXIST", buff, bufc);
        return;
    }

    struct channel *chn = select_channel(fargs[1]);
    if (!chn)
    {
        safe_str("#-1 CHANNEL DOES NOT EXIST", buff, bufc);
        return;
    }

    struct comuser *user;

    int i;
    int onchannel = 0;
    if (Wizard(player))
    {
        onchannel = 1;
    }
    else
    {
        for (i = 0; i < c->numchannels; i++)
        {
            user = select_user(chn, player);
            if (user)
            {
                onchannel = 1;
                break;
            }
        }
    }

    if (!onchannel)
    {
        safe_str("#-1 PERMISSION DENIED", buff, bufc);
        return;
    }

    for (i = 0; i < c->numchannels; i++)
    {
        user = select_user(chn, victim);
        if (user)
        {
            safe_str(user->title, buff, bufc);
            return;
        }
    }
    safe_str("#-1 PLAYER NOT ON THAT CHANNEL", buff, bufc);
}

// Returns a player's comsys alias for a named channel.
//
FUNCTION(fun_comalias)
{
    if (!mudconf.have_comsys)
    {
        safe_str("#-1 COMSYS DISABLED", buff, bufc);
        return;
    }

    dbref victim = lookup_player(player, fargs[0], 1);
    comsys_t *c = get_comsys(player);

    if (victim == NOTHING)
    {
        safe_str("#-1 PLAYER DOES NOT EXIST", buff, bufc);
        return;
    }

    struct channel *chn = select_channel(fargs[1]);
    if (!chn)
    {
        safe_str("#-1 CHANNEL DOES NOT EXIST", buff, bufc);
        return;
    }

    int i;
    struct comuser *user;
    int onchannel = 0;
    if (Wizard(player))
    {
        onchannel = 1;
    }
    else
    {
        for (i = 0; i < c->numchannels; i++)
        {
            user = select_user(chn, player);
            if (user)
            {
                onchannel = 1;
                break;
            }
        }
    }

    if (!onchannel)
    {
        safe_str("#-1 PERMISSION DENIED", buff, bufc);
        return;
    }

    comsys_t *cc = get_comsys(victim);
    for (i = 0; i < cc->numchannels; i++)
    {
        if (!strcmp(fargs[1], cc->channels[i]))
        {
            safe_str(cc->alias + i * 6, buff, bufc);
            return;
        }
    }
    safe_str("#-1 PLAYER NOT ON THAT CHANNEL", buff, bufc);
}

// Returns a list of channels.
//
FUNCTION(fun_channels)
{
    if (!mudconf.have_comsys)
    {
        safe_str("#-1 COMSYS DISABLED", buff, bufc);
        return;
    }

    BOOL bFirst = TRUE;
    struct channel *chn;
    for (chn = (struct channel *)hash_firstentry(&mudstate.channel_htab);
         chn;
         chn = (struct channel *)hash_nextentry(&mudstate.channel_htab))
    {
        if (  Comm_All(player)
           || (chn->type & CHANNEL_PUBLIC)
           || chn->charge_who == player)
        {
            if (bFirst)
            {
                bFirst = FALSE;
            }
            else
            {
                safe_chr(' ', buff, bufc);
            }
            safe_str(chn->name, buff, bufc);
        }
    }
}
