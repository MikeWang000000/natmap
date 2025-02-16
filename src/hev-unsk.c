/*
 ============================================================================
 Name        : hev-unsk.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2022 - 2025 xyz
 Description : UDP NAT session keeper
 ============================================================================
 */

#include <stdio.h>
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-conf.h"
#include "hev-misc.h"
#include "hev-sock.h"
#include "hev-stun.h"
#include "hev-ufwd.h"
#include "hev-xnsk.h"

#include "hev-unsk.h"

static struct sockaddr_storage saddr;
static HevTask *task;
static int timeout;

static void
stun_handler (void)
{
    const char *ufwd = hev_conf_taddr ();

    if (ufwd) {
        hev_ufwd_run ((struct sockaddr *)&saddr);
    }
}

static void
unsk_run (void)
{
    const char *ufwd;
    const char *addr;
    const char *port;
    const char *stun;
    const char *sport;
    const char *iface;
    unsigned int mark;
    int type;
    int fd;

    type = hev_conf_type ();
    ufwd = hev_conf_taddr ();
    addr = hev_conf_baddr ();
    port = hev_conf_bport ();
    stun = hev_conf_stun ();
    sport = hev_conf_sport ();
    iface = hev_conf_iface ();
    mark = hev_conf_mark ();
    timeout = hev_conf_keep ();

    fd = hev_sock_client_udp (type, addr, port, stun, sport, iface, mark,
                              &saddr);
    if (fd < 0) {
        LOGV (E, "%s", "Start UDP keep-alive service failed.");
        return;
    }
    close (fd);

    hev_stun_run ((struct sockaddr *)&saddr, stun_handler);

    do {
        if (hev_task_sleep (timeout) > 0) {
            break;
        }
        hev_stun_run (NULL, stun_handler);
    } while (timeout);

    if (ufwd) {
        hev_ufwd_kill ();
    }
}

static void
task_entry (void *data)
{
    for (;;) {
        unsk_run ();
        hev_task_sleep (5000);
    }
}

static void
unsk_kill (void)
{
    timeout = 0;
    if (task) {
        hev_task_wakeup (task);
    }
}

void
hev_unsk_run (void)
{
    hev_xnsk_init (unsk_kill);

    task = hev_task_new (-1);
    hev_task_run (task, task_entry, NULL);
}
