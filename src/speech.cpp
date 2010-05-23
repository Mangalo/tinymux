// speech.cpp -- Commands which involve speaking.
//
// $Id: speech.cpp,v 1.24 2002-06-07 19:43:49 sdennis Exp $
//

#include "copyright.h"
#include "autoconf.h"
#include "config.h"
#include "externs.h"

#include "interface.h"
#include "match.h"
#include "powers.h"
#include "attrs.h"

extern char *FDECL(next_token, (char *, char));

static int idle_timeout_val(dbref player)
{
    // If IDLETIMEOUT attribute is not present, the value
    // returned will be zero.
    //
    dbref aowner;
    int aflags;
    char *ITbuffer = atr_get(player, A_IDLETMOUT, &aowner, &aflags);
    int idle_timeout = Tiny_atol(ITbuffer);
    free_lbuf(ITbuffer);
    return idle_timeout;
}

int sp_ok(dbref player)
{
    if (Gagged(player) && (!(Wizard(player)))) {
        notify(player, "Sorry. Gagged players cannot speak.");
        return 0;
    }

    if (!mudconf.robot_speak) {
        if (Robot(player) && !controls(player, Location(player))) {
            notify(player, "Sorry robots may not speak in public.");
            return 0;
        }
    }
    if (Auditorium(Location(player))) {
        if (!could_doit(player, Location(player), A_LSPEECH)) {
            notify(player, "Sorry, you may not speak in this place.");
            return 0;
        }
    }
    return 1;
}

void wall_broadcast(int target, dbref player, char *message)
{
    DESC *d;
    DESC_ITER_CONN(d)
    {
        switch (target)
        {
        case SHOUT_WIZARD:

            if (Wizard(d->player))
            {
                notify_with_cause(d->player, player, message);
            }
            break;

        case SHOUT_ADMIN:

            if (WizRoy(d->player))
            {
                notify_with_cause(d->player, player, message);
            }
            break;

        default:

            notify_with_cause(d->player, player, message);
            break;
        }
    }
}

static void say_shout(int target, const char *prefix, int flags, dbref player, char *message)
{
    if (flags & SAY_NOTAG)
        wall_broadcast(target, player, tprintf("%s%s", Name(player), message));
    else
        wall_broadcast(target, player, tprintf("%s%s%s", prefix, Name(player), message));
}

static const char *announce_msg = "Announcement: ";
static const char *broadcast_msg = "Broadcast: ";
static const char *admin_msg = "Admin: ";

void do_think(dbref player, dbref cause, int key, char *message)
{
    char *str, *buf, *bp;

    buf = bp = alloc_lbuf("do_think");
    str = message;
    TinyExec(buf, &bp, 0, player, cause, EV_FCHECK | EV_EVAL | EV_TOP, &str, (char **)NULL, 0);
    *bp = '\0';
    notify(player, buf);
    free_lbuf(buf);
}

void do_say(dbref player, dbref cause, int key, char *message)
{
    dbref loc;
    char *buf2, *bp;
    int say_flags, depth;

    /*
     * Convert prefix-coded messages into the normal type
     */

    say_flags = key & (SAY_NOTAG | SAY_HERE | SAY_ROOM | SAY_HTML);
    key &= ~(SAY_NOTAG | SAY_HERE | SAY_ROOM | SAY_HTML);

    if (key == SAY_PREFIX) {
        switch (*message++) {
        case '"':
            key = SAY_SAY;
            break;
        case ':':
            if (*message == ' ') {
                message++;
                key = SAY_POSE_NOSPC;
            } else {
                key = SAY_POSE;
            }
            break;
        case ';':
            key = SAY_POSE_NOSPC;
            break;
        case '\\':
            key = SAY_EMIT;
            break;
        default:
            return;
        }
    }
    /*
     * Make sure speaker is somewhere if speaking in a place
     */

    loc = where_is(player);
    switch (key) {
    case SAY_SAY:
    case SAY_POSE:
    case SAY_POSE_NOSPC:
    case SAY_EMIT:
        if (loc == NOTHING)
            return;
        if (!sp_ok(player))
            return;
    }

    /*
     * Send the message on its way
     */

    switch (key)
    {
    case SAY_SAY:
        notify_saypose(player, tprintf("You say, \"%s\"", message));
        notify_except(loc, player, player, tprintf("%s says, \"%s\"", Name(player), message), MSG_SAYPOSE);
        break;

    case SAY_POSE:
        notify_all_from_inside_saypose(loc, player, tprintf("%s %s", Name(player), message));
        break;

    case SAY_POSE_NOSPC:
        notify_all_from_inside_saypose(loc, player, tprintf("%s%s", Name(player), message));
        break;

    case SAY_EMIT:
        if ((say_flags & SAY_HERE) || (say_flags & SAY_HTML) || !say_flags)
        {
            if (say_flags & SAY_HTML)
            {
                notify_all_from_inside_html(loc, player, message);
            }
            else
            {
                notify_all_from_inside(loc, player, message);
            }
        }
        if (say_flags & SAY_ROOM)
        {
            if ((Typeof(loc) == TYPE_ROOM) && (say_flags & SAY_HERE))
            {
                return;
            }
            depth = 0;
            while ((Typeof(loc) != TYPE_ROOM) && (depth++ < 20))
            {
                loc = Location(loc);
                if ((loc == NOTHING) || (loc == Location(loc)))
                {
                    return;
                }
            }
            if (Typeof(loc) == TYPE_ROOM)
            {
                notify_all_from_inside(loc, player, message);
            }
        }
        break;

    case SAY_SHOUT:
        switch (*message)
        {
        case ':':
            message[0] = ' ';
            say_shout(0, announce_msg, say_flags, player, message);
            break;

        case ';':
            message++;
            say_shout(0, announce_msg, say_flags, player, message);
            break;

        case '"':
            message++;

        default:
            buf2 = alloc_lbuf("do_say.shout");
            bp = buf2;
            safe_str(" shouts, \"", buf2, &bp);
            safe_str(message, buf2, &bp);
            safe_chr('"', buf2, &bp);
            *bp = '\0';
            say_shout(0, announce_msg, say_flags, player, buf2);
            free_lbuf(buf2);
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "SHOUT");
        log_name(player);
        log_text(" shouts: ");
        log_text(message);
        ENDLOG;
        break;

    case SAY_WIZSHOUT:
        switch (*message) {
        case ':':
            message[0] = ' ';
            say_shout(SHOUT_WIZARD, broadcast_msg, say_flags, player,
                  message);
            break;
        case ';':
            message++;
            say_shout(SHOUT_WIZARD, broadcast_msg, say_flags, player,
                  message);
            break;
        case '"':
            message++;
        default:
            buf2 = alloc_lbuf("do_say.wizshout");
            bp = buf2;
            safe_str((char *)" says, \"", buf2, &bp);
            safe_str(message, buf2, &bp);
            safe_chr('"', buf2, &bp);
            *bp = '\0';
            say_shout(SHOUT_WIZARD, broadcast_msg, say_flags, player,
                  buf2);
            free_lbuf(buf2);
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "BCAST");
        log_name(player);
        log_text((char *)" broadcasts: '");
        log_text(message);
        log_text((char *)"'");
        ENDLOG;
        break;

    case SAY_ADMINSHOUT:
        switch (*message) {
        case ':':
            message[0] = ' ';
            say_shout(SHOUT_ADMIN, admin_msg, say_flags, player,
                  message);
            break;
        case ';':
            message++;
            say_shout(SHOUT_ADMIN, admin_msg, say_flags, player,
                  message);
            break;
        case '"':
            message++;
        default:
            buf2 = alloc_lbuf("do_say.adminshout");
            bp = buf2;
            safe_str((char *)" says, \"", buf2, &bp);
            safe_str(message, buf2, &bp);
            safe_chr('"', buf2, &bp);
            *bp = '\0';
            say_shout(SHOUT_ADMIN, admin_msg, say_flags, player,
                  buf2);
            free_lbuf(buf2);
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "ASHOUT");
        log_name(player);
        log_text(" yells: ");
        log_text(message);
        ENDLOG;
        break;

    case SAY_WALLPOSE:
        if (say_flags & SAY_NOTAG)
        {
            wall_broadcast(0, player, tprintf("%s %s", Name(player), message));
        }
        else
        {
            wall_broadcast(0, player, tprintf("Announcement: %s %s", Name(player), message));
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "SHOUT");
        log_name(player);
        log_text(" WALLposes: ");
        log_text(message);
        ENDLOG;
        break;

    case SAY_WIZPOSE:
        if (say_flags & SAY_NOTAG)
        {
            wall_broadcast(SHOUT_WIZARD, player, tprintf("%s %s", Name(player), message));
        }
        else
        {
            wall_broadcast(SHOUT_WIZARD, player, tprintf("Broadcast: %s %s", Name(player), message));
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "BCAST");
        log_name(player);
        log_text(" WIZposes: ");
        log_text(message);
        ENDLOG;
        break;

    case SAY_WALLEMIT:
        if (say_flags & SAY_NOTAG)
        {
            wall_broadcast(0, player, tprintf("%s", message));
        }
        else
        {
            wall_broadcast(0, player, tprintf("Announcement: %s", message));
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "SHOUT");
        log_name(player);
        log_text(" WALLemits: ");
        log_text(message);
        ENDLOG;
        break;

    case SAY_WIZEMIT:
        if (say_flags & SAY_NOTAG)
        {
            wall_broadcast(SHOUT_WIZARD, player, tprintf("%s", message));
        }
        else
        {
            wall_broadcast(SHOUT_WIZARD, player, tprintf("Broadcast: %s", message));
        }
        STARTLOG(LOG_SHOUTS, "WIZ", "BCAST");
        log_name(player);
        log_text(" WIZemit: ");
        log_text(message);
        ENDLOG;
        break;
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_page: Handle the page command.
 * * Page-pose code from shadow@prelude.cc.purdue.
 */

static void page_return(dbref player, dbref target, const char *tag, int anum, const char *dflt)
{
    dbref aowner;
    int aflags;
    char *str, *str2, *buf, *bp;

    str = atr_pget(target, anum, &aowner, &aflags);
    if (*str)
    {
        str2 = bp = alloc_lbuf("page_return");
        buf = str;
        TinyExec(str2, &bp, 0, target, player, EV_FCHECK | EV_EVAL | EV_TOP | EV_NO_LOCATION, &buf, (char **)NULL, 0);
        *bp = '\0';
        if (*str2)
        {
            CLinearTimeAbsolute ltaNow;
            ltaNow.GetLocal();
            FIELDEDTIME ft;
            ltaNow.ReturnFields(&ft);

            notify_with_cause_ooc(player, target, tprintf("%s message from %s: %s", tag, Name(target), str2));
            notify_with_cause_ooc(target, player, tprintf("[%d:%02d] %s message sent to %s.", ft.iHour, ft.iMinute, tag, Name(player)));
        }
        free_lbuf(str2);
    }
    else if (dflt && *dflt)
    {
        notify_with_cause_ooc(player, target, dflt);
    }
    free_lbuf(str);
}

static int page_check(dbref player, dbref target)
{
    if (!payfor(player, Guest(player) ? 0 : mudconf.pagecost))
    {
        notify(player, tprintf("You don't have enough %s.", mudconf.many_coins));
    }
    else if (!Connected(target))
    {
        page_return(player, target, "Away", A_AWAY,
            tprintf("Sorry, %s is not connected.", Name(target)));
    }
    else if (!could_doit(player, target, A_LPAGE))
    {
        if (  Can_Hide(target)
           && Hidden(target)
           && !See_Hidden(player))
        {
            page_return(player, target, "Away", A_AWAY,
                tprintf("Sorry, %s is not connected.", Name(target)));
        }
        else
        {
            page_return(player, target, "Reject", A_REJECT,
                tprintf("Sorry, %s is not accepting pages.", Name(target)));
        }
    }
    else if (!could_doit(target, player, A_LPAGE))
    {
        if (Wizard(player))
        {
            notify(player, tprintf("Warning: %s can't return your page.", Name(target)));
            return 1;
        }
        else
        {
            notify(player, tprintf("Sorry, %s can't return your page.", Name(target)));
            return 0;
        }
    }
    else
    {
        return 1;
    }
    return 0;
}

//
// The combinations are:
//
//           nargs  arg1[0]  arg2[0]
//   ''        1      '\0'    '\0'      Report LastPaged to player.
//   'a'       1      'a'     '\0'      Page LastPaged with A
//   'a='      2      'a'     '\0'      Page A. LastPaged <- A
//   '=b'      2      '\0'    'b'       Page LastPaged with B
//   'a=b'     2      'a'     'b'       Page A with B. LastPaged <- A
//   'a=b1=[b2=]*...'                   All treated the same as 'a=b'.
//
void do_page
(
    dbref player,
    dbref cause,
    int   key,
    int   nargs,
    char *arg1,
    char *arg2
)
{
    int   nPlayers = 0;
    dbref aPlayers[(LBUF_SIZE+1)/2];

    // Either we have been given a recipient list, or we are relying on an
    // existing A_LASTPAGE.
    //
    BOOL bModified = FALSE;
    if (  nargs == 2
       && arg1[0] != '\0')
    {
        // Need to decode requested recipients.
        //
        TINY_STRTOK_STATE tts;
        Tiny_StrTokString(&tts, arg1);
        Tiny_StrTokControl(&tts, ", ");
        char *p;
        for (p = Tiny_StrTokParse(&tts); p; p = Tiny_StrTokParse(&tts))
        {
            dbref target = lookup_player(player,p, 1);
            if (target != NOTHING)
            {
                aPlayers[nPlayers++] = target;
            }
            else
            {
                notify(player, tprintf("I don't recognize \"%s\".", p));
            }
        }
        bModified = TRUE;
    }
    else
    {
        // Need to decode the A_LASTPAGE.
        //
        dbref aowner;
        int   aflags;
        char *pLastPage = atr_get(player, A_LASTPAGE, &aowner, &aflags);

        TINY_STRTOK_STATE tts;
        Tiny_StrTokString(&tts, pLastPage);
        Tiny_StrTokControl(&tts, " ");
        char *p;
        for (p = Tiny_StrTokParse(&tts); p; p = Tiny_StrTokParse(&tts))
        {
            dbref target = Tiny_atol(p);
            if (  Good_obj(target)
               && isPlayer(target))
            {
                aPlayers[nPlayers++] = target;
            }
            else
            {
                notify(player, tprintf("I don't recognized #%d.", target));
                bModified = TRUE;
            }
        }
        free_lbuf(pLastPage);
    }

    int nValid = nPlayers;

    // Remove duplicate dbrefs.
    //
    int i;
    for (i = 0; i < nPlayers-1; i++)
    {
        if (aPlayers[i] != NOTHING)
        {
            int j;
            for (j = i+1; j < nPlayers; j++)
            {
                if (aPlayers[j] == aPlayers[i])
                {
                    aPlayers[j] = NOTHING;
                    bModified = TRUE;
                    nValid--;
                }
            }
        }
    }

    // If we are doing more than reporting, we have some other dbref
    // validation to do.
    //
    if (  nargs == 2
       || arg1[0] != '\0')
    {
        for (i = 0; i < nPlayers; i++)
        {
            if (  aPlayers[i] != NOTHING
               && !page_check(player, aPlayers[i]))
            {
                aPlayers[i] = NOTHING;
                bModified = TRUE;
                nValid--;
            }
        }
    }

    if (bModified)
    {
        // Our aPlayers could be different than the one encoded on A_LASTPAGE.
        // Update the database.
        //
        ITB itb;
        char *pBuff = alloc_lbuf("do_page.lastpage");
        char *pBufc = pBuff;
        IntegerToBuffer_Init(&itb, pBuff, &pBufc);
        int i;
        for (i = 0; i < nPlayers; i++)
        {
            if (  aPlayers[i] != NOTHING
               && !IntegerToBuffer_Add(&itb, aPlayers[i]))
            {
                break;
            }
        }
        IntegerToBuffer_Final(&itb);
        atr_add_raw(player, A_LASTPAGE, pBuff);
        free_lbuf(pBuff);
    }

    // Verify that the recipient list isn't empty.
    //
    if (nValid == 0)
    {
        if (  nargs == 1
           && arg1[0] == '\0')
        {
            notify(player, "You have not paged anyone.");
        }
        else
        {
            notify(player, "No one to page.");
        }
        return;
    }

    // Build a friendly representation of the recipient list.
    //
    char *aFriendly = alloc_lbuf("do_page.friendly");
    char *pFriendly = aFriendly;

    if (nValid > 1)
    {
        safe_chr('(', aFriendly, &pFriendly);
    }
    BOOL bFirst = TRUE;
    for (i = 0; i < nPlayers; i++)
    {
        if (aPlayers[i] != NOTHING)
        {
            if (bFirst)
            {
                bFirst = FALSE;
            }
            else
            {
                safe_copy_buf(", ", 2, aFriendly, &pFriendly);
            }
            safe_str(Name(aPlayers[i]), aFriendly, &pFriendly);
        }
    }
    if (nValid > 1)
    {
        safe_chr(')', aFriendly, &pFriendly);
    }
    *pFriendly = '\0';

    // We may be able to proceed directly to the reporting case.
    //
    if (  nargs == 1
       && arg1[0] == '\0')
    {
        notify(player, tprintf("You last paged %s.", aFriendly));
        free_lbuf(aFriendly);
        return;
    }

    // Build messages.
    //
    char *omessage = alloc_lbuf("do_page.omessage");
    char *imessage = alloc_lbuf("do_page.imessage");
    char *omp = omessage;
    char *imp = imessage;

    char *pMessage;
    if (nargs == 1)
    {
        // 'page A' form.
        //
        pMessage = arg1;
    }
    else
    {
        // 'page A=', 'page =B', and 'page A=B' forms.
        //
        pMessage = arg2;
    }

    switch (*pMessage)
    {
    case '\0':
        // 'page A=' form.
        //
        if (nValid == 1)
        {
            safe_tprintf_str(omessage, &omp, "From afar, %s pages you.",
                Name(player));
        }
        else
        {
            safe_tprintf_str(omessage, &omp, "From afar, %s pages %s.",
                Name(player), aFriendly);
        }
        safe_tprintf_str(imessage, &imp, "You page %s.", aFriendly);
        break;

    case ':':
        pMessage++;
        safe_str("From afar, ", omessage, &omp);
        if (nValid > 1)
        {
            safe_tprintf_str(omessage, &omp, "to %s: ", aFriendly);
        }
        safe_tprintf_str(omessage, &omp, "%s %s", Name(player), pMessage);
        safe_tprintf_str(imessage, &imp, "Long distance to %s: %s %s",
            aFriendly, Name(player), pMessage);
        break;

    case ';':
        pMessage++;
        safe_str("From afar, ", omessage, &omp);
        if (nValid > 1)
        {
            safe_tprintf_str(omessage, &omp, "to %s: ", aFriendly);
        }
        safe_tprintf_str(omessage, &omp, "%s%s", Name(player), pMessage);
        safe_tprintf_str(imessage, &imp, "Long distance to %s: %s%s",
            aFriendly, Name(player), pMessage);
        break;

    case '"':
        pMessage++;

        // FALL THROUGH

    default:
        if (nValid > 1)
        {
            safe_tprintf_str(omessage, &omp, "To %s, ", aFriendly);
        }
        safe_tprintf_str(omessage, &omp, "%s pages: %s", Name(player),
            pMessage);
        safe_tprintf_str(imessage, &imp, "You paged %s with '%s'.",
            aFriendly, pMessage);
        break;
    }
    free_lbuf(aFriendly);

    // Send message to recipients.
    //
    for (i = 0; i < nPlayers; i++)
    {
        dbref target = aPlayers[i];
        if (target != NOTHING)
        {
            notify_with_cause_ooc(target, player, omessage);
            if (fetch_idle(target) >= idle_timeout_val(target))
            {
                page_return(player, target, "Idle", A_IDLE, NULL);
            }
        }
    }
    free_lbuf(omessage);

    // Send message to sender.
    //
    notify(player, imessage);
    free_lbuf(imessage);
}

/*
 * ---------------------------------------------------------------------------
 * * do_pemit: Messages to specific players, or to all but specific players.
 */

void whisper_pose(dbref player, dbref target, char *message)
{
    char *buff;

    buff = alloc_lbuf("do_pemit.whisper.pose");
    StringCopy(buff, Name(player));
    notify(player, tprintf("%s senses \"%s%s\"", Name(target), buff, message));
    notify_with_cause(target, player, tprintf("You sense %s%s", buff, message));
    free_lbuf(buff);
}

void do_pemit_single
(
    dbref player,
    int key,
    BOOL bDoContents,
    int pemit_flags,
    char *recipient,
    int chPoseType,
    char *message
)
{
    dbref target, loc;
    char *buf2, *bp;
    int depth;
    BOOL ok_to_do = FALSE;

    switch (key)
    {
    case PEMIT_FSAY:
    case PEMIT_FPOSE:
    case PEMIT_FPOSE_NS:
    case PEMIT_FEMIT:
        target = match_controlled(player, recipient);
        if (target == NOTHING)
        {
            return;
        }
        ok_to_do = TRUE;
        break;

    default:
        init_match(player, recipient, TYPE_PLAYER);
        match_everything(0);
        target = match_result();
    }

    switch (target)
    {
    case NOTHING:
        switch (key)
        {
        case PEMIT_WHISPER:
            notify(player, "Whisper to whom?");
            break;

        case PEMIT_PEMIT:
            notify(player, "Emit to whom?");
            break;

        case PEMIT_OEMIT:
            notify(player, "Emit except to whom?");
            break;

        default:
            notify(player, "Sorry.");
            break;
        }
        break;

    case AMBIGUOUS:
        notify(player, "I don't know who you mean!");
        break;

    default:

        // Enforce locality constraints.
        //
        if (  !ok_to_do
           && (  nearby(player, target)
              || Long_Fingers(player)
              || Controls(player, target)))
        {
            ok_to_do = TRUE;
        }
        if (  !ok_to_do
           && key == PEMIT_PEMIT
           && isPlayer(target)
           && mudconf.pemit_players)
        {
            if (!page_check(player, target))
            {
                return;
            }
            ok_to_do = TRUE;
        }
        if (  !ok_to_do
           && (  !mudconf.pemit_any
              || key != PEMIT_PEMIT))
        {
            notify(player, "You are too far away to do that.");
            return;
        }
        if (  bDoContents
           && !Controls(player, target)
           && !mudconf.pemit_any)
        {
            notify(player, "Permission denied.");
            return;
        }
        loc = where_is(target);

        switch (key)
        {
        case PEMIT_PEMIT:
            if (bDoContents)
            {
                if (Has_contents(target))
                {
                    notify_all_from_inside(target, player, message);
                }
            }
            else
            {
                if (pemit_flags & PEMIT_HTML)
                {
                    notify_with_cause_html(target, player, message);
                }
                else
                {
                    notify_with_cause(target, player, message);
                }
            }
            break;

        case PEMIT_OEMIT:
            notify_except(Location(target), player, target, message, 0);
            break;

        case PEMIT_WHISPER:
            if (  isPlayer(target)
               && !Connected(target))
            {
                page_return(player, target, "Away", A_AWAY,
                    tprintf("Sorry, %s is not connected.", Name(target)));
                return;
            }
            switch (chPoseType)
            {
            case ':':
                whisper_pose(player, target, message);
                break;

            case ';':
                message++;
                whisper_pose(player, target, message);
                break;

            case '"':
                message++;

            default:
                notify(player, tprintf("You whisper \"%s\" to %s.", message,
                    Name(target)));
                notify_with_cause(target, player,
                    tprintf("%s whispers \"%s\"", Name(player), message));
            }
            if (  !mudconf.quiet_whisper
               && !Wizard(player))
            {
                loc = where_is(player);
                if (loc != NOTHING)
                {
                    buf2 = alloc_lbuf("do_pemit.whisper.buzz");
                    bp = buf2;
                    safe_str(Name(player), buf2, &bp);
                    safe_str((char *)" whispers something to ", buf2, &bp);
                    safe_str(Name(target), buf2, &bp);
                    *bp = '\0';
                    notify_except2(loc, player, player, target, buf2);
                    free_lbuf(buf2);
                }
            }
            break;

        case PEMIT_FSAY:
            notify(target, tprintf("You say, \"%s\"", message));
            if (loc != NOTHING)
            {
                notify_except(loc, player, target,
                    tprintf("%s says, \"%s\"", Name(target), message), 0);
            }
            break;

        case PEMIT_FPOSE:
            notify_all_from_inside(loc, player, tprintf("%s %s", Name(target),
                message));
            break;

        case PEMIT_FPOSE_NS:
            notify_all_from_inside(loc, player, tprintf("%s%s", Name(target),
                message));
            break;

        case PEMIT_FEMIT:
            if (  (pemit_flags & PEMIT_HERE)
               || !pemit_flags)
            {
                notify_all_from_inside(loc, player, message);
            }
            if (pemit_flags & PEMIT_ROOM)
            {
                if (  isRoom(loc)
                   && (pemit_flags & PEMIT_HERE))
                {
                    return;
                }
                depth = 0;
                while (  !isRoom(loc)
                      && depth++ < 20)
                {
                    loc = Location(loc);
                    if (  loc == NOTHING
                       || loc == Location(loc))
                    {
                        return;
                    }
                }
                if (isRoom(loc))
                {
                    notify_all_from_inside(loc, player, message);
                }
            }
            break;
        }
    }
}

void do_pemit_list
(
    dbref player,
    int key,
    BOOL bDoContents,
    int pemit_flags,
    char *list,
    int chPoseType,
    char *message
)
{
    // Send a message to a list of dbrefs. The list is destructively
    // modified.
    //
    if (  message[0] == '\0'
       || list[0] == '\0')
    {
        return;
    }

    char *p;
    TINY_STRTOK_STATE tts;
    Tiny_StrTokString(&tts, list);
    Tiny_StrTokControl(&tts, " ");
    for (p = Tiny_StrTokParse(&tts); p; p = Tiny_StrTokParse(&tts))
    {
        do_pemit_single(player, key, bDoContents, pemit_flags, p, chPoseType,
            message);
    }
}

void do_pemit
(
    dbref player,
    dbref cause,
    int   key,
    int   nargs,
    char *recipient,
    char *message
)
{
    if (nargs < 2)
    {
        return;
    }

    // Decode PEMIT_CONENTS and PEMIT_LIST and remove from key.
    //
    BOOL bDoContents = FALSE;
    if (key & PEMIT_CONTENTS)
    {
        bDoContents = TRUE;
    }
    BOOL bDoList = FALSE;
    if (key & PEMIT_LIST)
    {
        bDoList = TRUE;
    }
    key &= ~(PEMIT_CONTENTS |  PEMIT_LIST);


    // Decode PEMIT_HERE, PEMIT_ROOM, PEMIT_HTML and remove from key.
    //
    int mask = PEMIT_HERE | PEMIT_ROOM | PEMIT_HTML;
    int pemit_flags = key & mask;
    key &= ~mask;

    int chPoseType = *message;
    if (key == PEMIT_WHISPER && chPoseType == ':')
    {
        message[0] = ' ';
    }

    if (bDoList)
    {
        do_pemit_list(player, key, bDoContents, pemit_flags, recipient,
            chPoseType, message);
    }
    else
    {
        do_pemit_single(player, key, bDoContents, pemit_flags, recipient,
            chPoseType, message);
    }
}
