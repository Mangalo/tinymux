// plusmail.cpp -- Quicky module implementing Firan-like @email function.
//
// $Id$
//
// Rachel Blackman <sparks@noderunner.net>
//
// Config options:
//     mail_server               SMTP server to use (mail.bar.com)
//     mail_ehlo                 ehlo server (my.baz.com)
//     mail_sendaddr             return address (foo@bar.com)
//     mail_sendname             RFC822 'name' (FooMUSH)
//     mail_subject              Defaults subject link (FooMUSH Mail)
//

#include "autoconf.h"
#include "config.h"
#include "_build.h"

#if defined(FIRANMUX)

#ifndef WIN32
#include <netdb.h>
#endif

#include "externs.h"

/* Some basic Socket I/O crap I stole from another project of mine */

/* Write a formatted string to a socket */
static int mod_email_sock_printf(SOCKET sock, char *format, ...)
{
    va_list vargs;
    char mybuf[LBUF_SIZE];

    if (IS_INVALID_SOCKET(sock))
    {
        return 0;
    }

    va_start(vargs, format);
    mux_vsnprintf(mybuf, LBUF_SIZE, format, vargs);
    va_end(vargs);

    return SOCKET_WRITE(sock, &mybuf[0], strlen(mybuf), 0);
}

/* Read a line of input from the socket */
static int mod_email_sock_readline(SOCKET sock, char *buffer, int maxlen)
{
    if (IS_INVALID_SOCKET(sock))
    {
        return 0;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    // Wait up to 1 seconds.
    //
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;

    // Check for data before giving up.
    //
    if (IS_SOCKET_ERROR(select(sock+1, &read_fds, NULL, NULL, &tv)))
    {
        return 0;
    }

    if (!FD_ISSET(sock, &read_fds))
    {
        return 0;
    }

    bool done = false;
    bool possible_close = false;
    int  pos = 0;

    while (  !done
          && pos < maxlen)
    {
        char getme[2];

        int numread = SOCKET_READ(sock, &getme[0], 1, 0);
        if (  IS_SOCKET_ERROR(numread)
           || 0 == numread)
        {
            if (possible_close)
            {
                done = true;
            }
            else
            {
                FD_ZERO(&read_fds);
                FD_SET(sock, &read_fds);

                /* wait up to 5 seconds */
                tv.tv_sec = 1;
                tv.tv_usec = 0;

                /* Check for data before giving up. */
                if (IS_SOCKET_ERROR(select(sock+1, &read_fds, NULL, NULL, &tv)))
                {
                    done = true;
                }

                if (FD_ISSET(sock, &read_fds))
                {
                    possible_close = true;
                }
            }
        }
        else
        {
            possible_close = false;
            if (getme[0] != '\n')
            {
                buffer[pos++] = getme[0];
            }
            else
            {
                done = true;
            }
        }
    }
    buffer[pos] = '\0';

    return strlen(buffer);
}

/* Open a socket to a specific host/port */
static int mod_email_sock_open(const char *conhostname, int port, SOCKET *sock)
{
    struct hostent *conhost;
    struct sockaddr_in name;
    int addr_len;

    conhost = gethostbyname(conhostname);
    if (0 == conhost)
    {
        return -1;
    }

    name.sin_port = htons(port);
    name.sin_family = AF_INET;
    memcpy((char *)&name.sin_addr, (char *)conhost->h_addr, conhost->h_length);
    SOCKET mysock = socket(AF_INET, SOCK_STREAM, 0);
    addr_len = sizeof(name);
   
    if (connect(mysock, (struct sockaddr *)&name, addr_len) == -1)
    {
        return -2;
    }

    *sock = mysock;
 
    return 0;
}

static int mod_email_sock_close(SOCKET sock)
{
    return SOCKET_CLOSE(sock);
}

void do_plusemail(dbref executor, dbref cause, dbref enactor, int key,
                 int nfargs, char *arg1, char *arg2)
{
    UNUSED_PARAMETER(cause);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(nfargs);

    char inputline[LBUF_SIZE];

    if ('\0' == mudconf.mail_server[0])
    {
        notify(executor, "@email: Not configured");
        return;
    }

    if (!arg1 || !*arg1)
    {
        notify(executor, "@email: I don't know who you want to e-mail!");
        return;
    }

    if (!arg2 || !*arg2)
    {
        notify(executor, "@email: Not sending an empty e-mail!");
        return;
    }

    char *addy = alloc_lbuf("mod_email_do_email.headers");
    strcpy(addy, arg1);

    char *subject = strchr(addy,'/');
    if (subject)
    {
        *subject = '\0';
        subject++;
    }

    SOCKET mailsock = INVALID_SOCKET;
    int result = mod_email_sock_open(mudconf.mail_server, 25, &mailsock);

    if (-1 == result)
    {
        notify(executor, tprintf("@email: Unable to resolve hostname %s!",
            mudconf.mail_server));
        free_lbuf(addy);
        return;
    }
    else if (-2 == result)
    {

        // Periodically, we get a failed connect, for reasons which elude me.
        // In almost every case, an immediate retry works.  Therefore, we give
        // it one more shot, before we give up.
        //
        result = mod_email_sock_open(mudconf.mail_server, 25, &mailsock);
        if (0 != result)
        {
            notify(executor, "@email: Unable to connect to mailserver, aborting!");
            free_lbuf(addy);
            return;
        }

    }

    char *body = alloc_lbuf("mod_email_do_email.body");
    char *bodyptr = body;
    char *bodysrc = arg2;
    mux_exec(body, &bodyptr, executor, executor, executor,
        EV_STRIP_CURLY | EV_FCHECK | EV_EVAL, &bodysrc, (char **)NULL, 0);
    *bodyptr = 0;

    do
    {
        result = mod_email_sock_readline(mailsock, inputline, LBUF_SIZE - 1);
    } while (  0 == result
            || '-' == inputline[3]);

    if (-1 == result)
    {
        mod_email_sock_close(mailsock);
        notify(executor, "@email: Connection to mailserver lost.");
        return;
    }

    if (inputline[0] != '2')
    {
        mod_email_sock_close(mailsock);
        notify(executor, tprintf("@email: Invalid mailserver greeting (%s)",
            inputline));
    }
        

    mod_email_sock_printf(mailsock, "EHLO %s\r\n", mudconf.mail_ehlo);

    do
    {
        result = mod_email_sock_readline(mailsock, inputline, LBUF_SIZE - 1);
    } while (  0 == result
            || '-' == inputline[3]);
    
    if (-1 == result)
    {
        mod_email_sock_close(mailsock);
        notify(executor, "@email: Connection to mailserver lost.");
        return;
    }

    if ('2' != inputline[0])
    {
        notify(executor, tprintf("@email: Error response on EHLO (%s)",
            inputline));
    }

    mod_email_sock_printf(mailsock, "MAIL FROM:<%s>\r\n", mudconf.mail_sendaddr);   

    do
    {
        result = mod_email_sock_readline(mailsock, inputline, LBUF_SIZE - 1);
    } while (  0 == result
            || '-' == inputline[3]);

    if (-1 == result)
    {
        mod_email_sock_close(mailsock);
        notify(executor, "@email: Connection to mailserver lost.");
        return;
    }

    if ('2' != inputline[0])
    {
        notify(executor, tprintf("@email: Error response on MAIL FROM (%s)", 
            inputline));
    }

    mod_email_sock_printf(mailsock, "RCPT TO:<%s>\r\n", addy);

    do
    {
        result = mod_email_sock_readline(mailsock, inputline, LBUF_SIZE - 1);
    } while (  0 == result
            || '-' == inputline[3]);

    if (-1 == result)
    {
        mod_email_sock_close(mailsock);
        notify(executor, "@email: Connection to mailserver lost.");
        return;
    }

    if ('2' != inputline[0])
    {
        notify(executor, tprintf("@email: Error response on RCPT TO (%s)",
            inputline));
        return;
    }

    mod_email_sock_printf(mailsock, "DATA\r\n");

    do
    {
        result = mod_email_sock_readline(mailsock, inputline, LBUF_SIZE - 1);
    } while (  0 == result
            || '-' == inputline[3]);

    if (-1 == result)
    {
        mod_email_sock_close(mailsock);
        notify(executor, "@email: Connection to mailserver lost.");
        return;
    }

    if ('3' != inputline[0])
    {
        notify(executor, tprintf("@email: Error response on DATA (%s)",
            inputline));
        return;
    }

    mod_email_sock_printf(mailsock, "From: %s <%s>\r\n",  mudconf.mail_sendname, mudconf.mail_sendaddr);
    mod_email_sock_printf(mailsock, "To: %s\r\n", addy);
    mod_email_sock_printf(mailsock, "X-Mailer: TinyMUX %s\r\n", mudstate.short_ver);
    mod_email_sock_printf(mailsock, "Subject: %s\r\n\r\n", subject ? subject : mudconf.mail_subject);
    mod_email_sock_printf(mailsock, "%s\r\n", body);
    mod_email_sock_printf(mailsock, "\r\n.\r\n");

    do
    {
        result = mod_email_sock_readline(mailsock, inputline, LBUF_SIZE - 1);
        if (result > 0)
        {
            if (  '\n' == inputline[strlen(inputline) - 1]
               || '\r' == inputline[strlen(inputline) - 1])
            {
                inputline[strlen(inputline) - 1] = '0';
            }
        
            if (  '\n' == inputline[strlen(inputline) - 1]
               || '\r' == inputline[strlen(inputline) - 1])
            {
                inputline[strlen(inputline) - 1] = '0';
            }
        
            if (strlen(inputline) == 0)
            {
                result = 0;
            }
        }
    } while (  0 == result
            || '-' == inputline[3]);

    if (-1 == result)
    {
        mod_email_sock_close(mailsock);
        notify(executor, "@email: Connection to mailserver lost.");
        return;
    }

    if ('2' != inputline[0])
    {
        notify(executor, tprintf("@email: Message rejected (%s)",inputline));
    }
    else
    {
        notify(executor, tprintf("@email: Mail sent to %s (%s)", addy, &inputline[4])); 
    }

    mod_email_sock_printf(mailsock, "QUIT\n");
    mod_email_sock_close(mailsock);

    free_lbuf(addy);
    free_lbuf(body);
}

#endif // FIRANMUX
