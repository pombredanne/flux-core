#ifndef _FLUX_CORE_PMI_SIMPLE_SERVER_H
#define _FLUX_CORE_PMI_SIMPLE_SERVER_H

struct pmi_simple_server;

/* User-provided service implementation.
 * All return 0 on success, -1 on failure.
 */
struct pmi_simple_ops {
    int (*kvs_put)(void *arg, const char *kvsname,
                   const char *key, const char *val);
    int (*kvs_get)(void *arg, const char *kvsname,
                   const char *key, char *val, int len);
    int (*barrier_enter)(void *arg);
    int (*response_send)(void *client, const char *buf);
};

/* Create/destroy protocol engine.
 */
struct pmi_simple_server *pmi_simple_server_create (struct pmi_simple_ops *ops,
                                                    int appnum,
                                                    int universe_size,
                                                    int local_procs,
                                                    const char *kvsname,
                                                    void *arg);
void pmi_simple_server_destroy (struct pmi_simple_server *pmi);

/* Max buffer size needed to read a null-terminated request line,
 * including trailing newline.
 */
int pmi_simple_server_get_maxrequest (struct pmi_simple_server *pmi);

/* Put null-terminated request with sending client reference to protocol
 * engine.  The request should end with a newline.
 * Return 0 on success, -1 on failure.
 */
int pmi_simple_server_request (struct pmi_simple_server *pmi,
                               const char *buf, void *client);

/* Finalize a barrier.  Set rc to 0 for success, -1 for failure.
 */
int pmi_simple_server_barrier_complete (struct pmi_simple_server *pmi, int rc);

#endif /* ! _FLUX_CORE_PMI_SIMPLE_SERVER_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
