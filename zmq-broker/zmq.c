/* zmq.c - wrapper functions for zmq prototyping */

#define _GNU_SOURCE
#include <stdio.h>
#include <zmq.h>
#include <czmq.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <json/json.h>
#include <assert.h>

#include "zmq.h"
#include "util.h"
#include "log.h"
#include "cmb.h"

#ifndef ZMQ_DONTWAIT
#   define ZMQ_DONTWAIT   ZMQ_NOBLOCK
#endif
#ifndef ZMQ_RCVHWM
#   define ZMQ_RCVHWM     ZMQ_HWM
#endif
#ifndef ZMQ_SNDHWM
#   define ZMQ_SNDHWM     ZMQ_HWM
#endif
#if ZMQ_VERSION_MAJOR == 2
#   define more_t int64_t
#   define zmq_ctx_destroy(context) zmq_term(context)
#   define zmq_msg_send(msg,sock,opt) zmq_send (sock, msg, opt)
#   define zmq_msg_recv(msg,sock,opt) zmq_recv (sock, msg, opt)
#   define ZMQ_POLL_MSEC    1000        //  zmq_poll is usec
#elif ZMQ_VERSION_MAJOR == 3
#   define more_t int
#   define ZMQ_POLL_MSEC    1           //  zmq_poll is msec
#endif

/**
 ** zmq wrappers
 **/

int zpoll (zmq_pollitem_t *items, int nitems, long timeout)
{
    int rc;

    if ((rc = zmq_poll (items, nitems, timeout * ZMQ_POLL_MSEC)) < 0)
        err_exit ("zmq_poll");
    return rc;
}

void zconnect (zctx_t *zctx, void **sp, int type, char *uri, int hwm, char *id)
{
    *sp = zsocket_new (zctx, type);
    if (hwm != -1)
        zsocket_set_hwm (*sp, hwm);
    if (id != NULL)
        zsocket_set_identity (*sp, id);
    if (zsocket_connect (*sp, "%s", uri) < 0)
        err_exit ("zsocket_connect: %s", uri);
}

void zbind (zctx_t *zctx, void **sp, int type, char *uri, int hwm)
{
    *sp = zsocket_new (zctx, type);
    if (hwm != -1)
        zsocket_set_hwm (*sp, hwm);
    if (zsocket_bind (*sp, "%s", uri) < 0)
        err_exit ("zsocket_bind: %s", uri);
}

zmsg_t *zmsg_recv_fd (int fd, int flags)
{
    char *buf;
    int n;
    zmsg_t *msg;

    buf = xzmalloc (CMB_API_BUFSIZE);
    n = recv (fd, buf, CMB_API_BUFSIZE, flags);
    if (n < 0)
        goto error;
    if (n == 0) {
        errno = EPROTO;
        goto error;
    }
    msg = zmsg_decode ((byte *)buf, n);
    free (buf);
    return msg;
error:
    free (buf);
    return NULL;
}

int zmsg_send_fd (int fd, zmsg_t **msg)
{
    char *buf = NULL;
    int len;

    len = zmsg_encode (*msg, (byte **)&buf);
    if (len < 0) {
        errno = EPROTO;
        goto error;
    }
    if (send (fd, buf, len, 0) < len)
        goto error;
    free (buf);
    zmsg_destroy (msg);
    return 0;
error:
    if (buf)
        free (buf);
    return -1;
}


/**
 ** cmb messages
 **/

int cmb_msg_hopcount (zmsg_t *zmsg)
{
    int count = 0;
    zframe_t *zf;

    zf = zmsg_first (zmsg);
    while (zf && zframe_size (zf) != 0) {
        count++;
        zf = zmsg_next (zmsg); /* skip non-empty */
    }
    if (!zf)
        count = 0;
    return count;
}

static zframe_t *_tag_frame (zmsg_t *zmsg)
{
    zframe_t *zf;

    zf = zmsg_first (zmsg);
    while (zf && zframe_size (zf) != 0)
        zf = zmsg_next (zmsg); /* skip non-empty */
    if (zf)
        zf = zmsg_next (zmsg); /* skip empty */
    if (!zf)
        zf = zmsg_first (zmsg); /* rewind - there was no envelope */
    return zf;
}

static zframe_t *_json_frame (zmsg_t *zmsg)
{
    zframe_t *zf = _tag_frame (zmsg);

    return (zf ? zmsg_next (zmsg) : NULL);
}

static zframe_t *_data_frame (zmsg_t *zmsg)
{
    zframe_t *zf = _json_frame (zmsg);

    return (zf ? zmsg_next (zmsg) : NULL);
}

static zframe_t *_sender_frame (zmsg_t *zmsg)
{
    zframe_t *zf, *prev;

    zf = zmsg_first (zmsg);
    while (zf && zframe_size (zf) != 0) {
        prev = zf;
        zf = zmsg_next (zmsg);
    }
    return (zf ? prev : NULL);
}

int cmb_msg_decode (zmsg_t *zmsg, char **tagp, json_object **op,
                    void **datap, int *lenp)
{
    zframe_t *tag = _tag_frame (zmsg);
    zframe_t *json = zmsg_next (zmsg);
    zframe_t *data = zmsg_next (zmsg);

    if (!tag)
        goto eproto;
    if (tagp)
        *tagp = zframe_strdup (tag);
    if (op) {
        char *tmp = json ? zframe_strdup (json) : NULL;

        if (tmp) {
            *op = json_tokener_parse (tmp);
            free (tmp);
        } else
            *op = NULL;
    }
    if (datap && lenp) {
        if (data) {
            *lenp = zframe_size (data);
            *datap = xzmalloc (zframe_size (data));
            memcpy (*datap, zframe_data (data), zframe_size (data));
        } else {
            *lenp = 0;
            *datap = NULL;
        }
    }
    return 0;
eproto:
    errno = EPROTO;
    return -1;
}

int cmb_msg_recv (void *socket, char **tagp, json_object **op,
                    void **datap, int *lenp, int flags)
{
    zmsg_t *msg = NULL;

    if ((flags & ZMQ_DONTWAIT) && !zsocket_poll (socket, 0)) {
        errno = EAGAIN; 
        return -1;
    }
    if (!(msg = zmsg_recv (socket)))
        goto error;
    if (cmb_msg_decode (msg, tagp, op, datap, lenp) < 0)
        goto error;
    zmsg_destroy (&msg);
    return 0;
error:
    if (msg)
        zmsg_destroy (&msg);
    return -1;
}

int cmb_msg_recv_fd (int fd, char **tagp, json_object **op,
                     void **datap, int *lenp, int flags)
{
    zmsg_t *zmsg;

    zmsg = zmsg_recv_fd (fd, flags);
    if (!zmsg)
        goto error;
    if (cmb_msg_decode (zmsg, tagp, op, datap, lenp) < 0)
        goto error;
    zmsg_destroy (&zmsg);
    return 0;

error:
    if (zmsg)
        zmsg_destroy (&zmsg);
    return -1;
}

zmsg_t *cmb_msg_encode (char *tag, json_object *o, void *data, int len)
{
    zmsg_t *zmsg = NULL;

    if (!(zmsg = zmsg_new ()))
        err_exit ("zmsg_new");
    if (zmsg_addstr (zmsg, "%s", tag) < 0)
        err_exit ("zmsg_addstr");
    if (o) {
        if (zmsg_addstr (zmsg, "%s", json_object_to_json_string (o)) < 0)
            err_exit ("zmsg_addstr");
    }
    if (len > 0 && data != NULL) {
        assert (o != NULL);
        if (zmsg_addmem (zmsg, data, len) < 0)
            err_exit ("zmsg_addmem");
    }
    return zmsg;
}

void cmb_msg_send_long (void *sock, json_object *o, void *data, int len,
                        const char *fmt, ...)
{
    va_list ap;
    zmsg_t *zmsg;
    char *tag;
    int n;

    va_start (ap, fmt);
    n = vasprintf (&tag, fmt, ap);
    va_end (ap);
    if (n < 0)
        err_exit ("vasprintf");
   
    zmsg = cmb_msg_encode (tag, o, data, len);
    free (tag);
    if (zmsg_send (&zmsg, sock) < 0)
        err_exit ("zmsg_send");
}

void cmb_msg_send (void *sock, json_object *o, const char *fmt, ...)
{
    va_list ap;
    zmsg_t *zmsg;
    char *tag;
    int n;

    va_start (ap, fmt);
    n = vasprintf (&tag, fmt, ap);
    va_end (ap);
    if (n < 0)
        err_exit ("vasprintf");
   
    zmsg = cmb_msg_encode (tag, o, NULL, 0);
    free (tag);
    if (zmsg_send (&zmsg, sock) < 0)
        err_exit ("zmsg_send");
}

/* routed verison of above */
void cmb_msg_send_rt (void *sock, json_object *o, const char *fmt, ...)
{
    va_list ap;
    zmsg_t *zmsg;
    char *tag;
    int n;

    va_start (ap, fmt);
    n = vasprintf (&tag, fmt, ap);
    va_end (ap);
    if (n < 0)
        err_exit ("vasprintf");
   
    zmsg = cmb_msg_encode (tag, o, NULL, 0);
    free (tag);
    if (zmsg_pushmem (zmsg, NULL, 0) < 0)
        oom ();
    if (zmsg_send (&zmsg, sock) < 0)
        err_exit ("zmsg_send");
}

int cmb_msg_send_long_fd (int fd, json_object *o, void *data, int len,
                          const char *fmt, ...)
{
    va_list ap;
    zmsg_t *zmsg = NULL;
    char *tag = NULL;
    int n;

    va_start (ap, fmt);
    n = vasprintf (&tag, fmt, ap);
    va_end (ap);
    if (n < 0)
        err_exit ("vasprintf");

    zmsg = cmb_msg_encode (tag, o, data, len);
    if (zmsg_send_fd (fd, &zmsg) < 0) /* destroys msg on succes */
        goto error;
    free (tag);
    return 0;
error:
    if (zmsg)
        zmsg_destroy (&zmsg);
    if (tag)
        free (tag);
    return -1; 
}

int cmb_msg_send_fd (int fd, json_object *o, const char *fmt, ...)
{
    va_list ap;
    zmsg_t *zmsg = NULL;
    char *tag = NULL;
    int n;

    va_start (ap, fmt);
    n = vasprintf (&tag, fmt, ap);
    va_end (ap);
    if (n < 0)
        err_exit ("vasprintf");
   
    zmsg = cmb_msg_encode (tag, o, NULL, 0);
    if (zmsg_send_fd (fd, &zmsg) < 0) /* destroys msg on succes */
        goto error;
    free (tag);
    return 0;
error:
    if (zmsg)
        zmsg_destroy (&zmsg);
    if (tag)
        free (tag);
    return -1;
}

bool cmb_msg_match (zmsg_t *zmsg, const char *tag)
{
    zframe_t *zf = _tag_frame (zmsg);

    if (!zf)
        msg_exit ("cmb_msg_match: no tag in message");
    return zframe_streq (zf, tag);
}

bool cmb_msg_match_substr (zmsg_t *zmsg, const char *tag, char **restp)
{
    int taglen = strlen (tag);
    zframe_t *zf = _tag_frame (zmsg);
    char *ztag;
    int ztaglen;

    if (!zf)
        msg_exit ("cmb_msg_match: no tag in message");
    if (!(ztag = zframe_strdup (zf)))
        oom ();
    ztaglen = strlen (ztag); 
    if (ztaglen >= taglen && strncmp (tag, ztag, taglen) == 0) {
        if (restp) {
            memmove (ztag, ztag + taglen, ztaglen - taglen + 1);
            *restp = ztag;
        } else
            free (ztag);
        return true;
    }
    free (ztag);
    return false;
}

bool cmb_msg_match_sender (zmsg_t *zmsg, const char *sender)
{
    zframe_t *zf = _sender_frame (zmsg);

    if (!zf)
        msg_exit ("cmb_msg_match_sender: no envelope in message");
    return zframe_streq (zf, sender);
}

/* extract the first address in the envelope (sender uuid) */
char *cmb_msg_sender (zmsg_t *zmsg)
{
    zframe_t *zf = _sender_frame (zmsg);
    if (!zf) {
        msg ("cmb_msg_sender: empty envelope");
        return NULL;
    }
    return zframe_strdup (zf); /* caller must free */
}

char *cmb_msg_tag (zmsg_t *zmsg, bool shorten)
{
    zframe_t *zf = _tag_frame (zmsg);
    char *tag;
    if (!zf) {
        msg ("cmb_msg_tag: no tag frame");
        return NULL;
    }
    tag = zframe_strdup (zf); /* caller must free */
    if (tag && shorten) {
        char *p = strchr (tag, '.');
        if (p)
            *p = '\0';
    }
    return tag;
}

/* Replace request JSON with reply JSON.
 */
int cmb_msg_rep_json (zmsg_t *zmsg, json_object *o)
{
    const char *json = json_object_to_json_string (o);
    zframe_t *zf = _json_frame (zmsg);

    if (!zf) {
        msg ("%s: no JSON frame", __FUNCTION__);
        errno = EPROTO;
        return -1;
    }
    /* N.B. calls zmq_msg_init_size internally with unchecked return value */
    zframe_reset (zf, json, strlen (json));
    return 0;
}

/* Replace request JSON with error JSON.
 */
int cmb_msg_rep_errnum (zmsg_t *zmsg, int errnum)
{
    json_object *no, *o = NULL;
    zframe_t *zf = _json_frame (zmsg);
    const char *json;

    if (!zf) {
        msg ("%s: no JSON frame", __FUNCTION__);
        errno = EPROTO;
        goto error;
    }
    if (!(o = json_object_new_object ()))
        goto nomem;
    if (!(no = json_object_new_int (errnum)))
        goto nomem;
    json_object_object_add (o, "errnum", no);
    json = json_object_to_json_string (o);
    /* N.B. calls zmq_msg_init_size internally with unchecked return value */
    zframe_reset (zf, json, strlen (json));
    json_object_put (o);
    return 0;
nomem:
    errno = ENOMEM;
error:
    if (o)
        json_object_put (o);
    return -1;
}

int cmb_msg_datacpy (zmsg_t *zmsg, char *buf, int len)
{
    zframe_t *zf = _data_frame (zmsg);

    if (!zf) {
        msg ("cmb_msg_makenak: no data frame");
        return -1;
    }
    if (zframe_size (zf) > len) {
        msg ("%s: buffer too small", __FUNCTION__);
        return -1;
    }
    memcpy (buf, zframe_data (zf), zframe_size (zf));
    return zframe_size (zf);
}

void cmb_msg_send_errnum (zmsg_t **zmsg, void *socket, int errnum)
{
    if (cmb_msg_rep_errnum (*zmsg, errnum) < 0)
        goto done;
    if (zmsg_send (zmsg, socket) < 0) {
        err ("%s: zmsg_send", __FUNCTION__);
        goto done;
    }
done:
    if (zmsg)
        zmsg_destroy (zmsg);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

