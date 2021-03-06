#ifndef _FLUX_CORE_DISPATCH_H
#define _FLUX_CORE_DISPATCH_H

#include "message.h"
#include "handle.h"

typedef struct flux_msg_handler flux_msg_handler_t;

typedef void (*flux_msg_handler_f)(flux_t h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg);

flux_msg_handler_t *flux_msg_handler_create (flux_t h,
                                             const struct flux_match match,
                                             flux_msg_handler_f cb, void *arg);

void flux_msg_handler_destroy (flux_msg_handler_t *w);

void flux_msg_handler_start (flux_msg_handler_t *w);
void flux_msg_handler_stop (flux_msg_handler_t *w);


struct flux_msg_handler_spec {
    int typemask;
    char *topic_glob;
    flux_msg_handler_f cb;
    flux_msg_handler_t *w;
};
#define FLUX_MSGHANDLER_TABLE_END { 0, NULL, NULL }

int flux_msg_handler_addvec (flux_t h, struct flux_msg_handler_spec tab[],
                             void *arg);
void flux_msg_handler_delvec (struct flux_msg_handler_spec tab[]);


/* Give control back to the reactor until a message matching 'match'
 * is queued in the handle.  This will return -1 with errno = EINVAL
 * if called from a reactor handler that is not running in as a coprocess.
 * Currently only message handlers are started as coprocesses, if the
 * handle has FLUX_O_COPROC set.  This is used internally by flux_recv().
 */
int flux_sleep_on (flux_t h, struct flux_match match);

#endif /* !_FLUX_CORE_DISPATCH_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
