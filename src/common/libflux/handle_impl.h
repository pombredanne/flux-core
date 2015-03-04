#ifndef _FLUX_CORE_HANDLE_IMPL_H
#define _FLUX_CORE_HANDLE_IMPL_H

#include <stdbool.h>
#include <czmq.h>

#include "handle.h"

/**
 ** Only handle implementation stuff below.
 ** Flux_t handle users should not use these interfaces.
 */

typedef struct reactor_struct *reactor_t;

struct flux_handle_ops {
    int         (*sendmsg)(void *impl, zmsg_t **zmsg);
    zmsg_t *    (*recvmsg)(void *impl, bool nonblock);
    int         (*putmsg)(void *impl, zmsg_t **zmsg);

    int         (*event_subscribe)(void *impl, const char *topic);
    int         (*event_unsubscribe)(void *impl, const char *topic);

    int         (*rank)(void *impl);

    zctx_t *    (*get_zctx)(void *impl);

    int         (*reactor_start)(void *impl);
    void        (*reactor_stop)(void *impl, int rc);
    int         (*reactor_fd_add)(void *impl, int fd, int events,
                                  FluxFdHandler, void *arg);
    void        (*reactor_fd_remove)(void *impl, int fd, int events);
    int         (*reactor_zs_add)(void *impl, void *zs, int events,
                                     FluxZsHandler cb, void *arg);
    void        (*reactor_zs_remove)(void *impl, void *zs, int events);

    int         (*reactor_tmout_add)(void *impl, unsigned long ms, bool oneshot,
                                     FluxTmoutHandler cb, void *arg);
    void        (*reactor_tmout_remove)(void *impl, int timer_id);
    int         (*reactor_msg_add)(void *impl, FluxMsgHandler cb, void *arg);
    void        (*reactor_msg_remove)(void *impl);

    void        (*impl_destroy)(void *impl);
};

flux_t flux_handle_create (void *impl, const struct flux_handle_ops *ops, int flags);
void flux_handle_destroy (flux_t *hp);

zctx_t *flux_get_zctx (flux_t h);
reactor_t flux_get_reactor (flux_t h);
reactor_t flux_reactor_create (void *impl, const struct flux_handle_ops *ops);
void flux_reactor_destroy (reactor_t r);

#endif /* !_FLUX_CORE_HANDLE_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
