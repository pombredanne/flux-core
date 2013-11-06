#ifndef KVS_H
#define KVS_H
typedef struct kvsctx_struct *kvsctx_t;
typedef struct kvsdir_struct *kvsdir_t;

typedef kvsctx_t (KVSGetCtxF(void *h));

typedef void (KVSSetF(const char *key, json_object *val, void *arg,int errnum));
typedef void (KVSSetDirF(const char *key, kvsdir_t dir, void *arg, int errnum));
typedef void (KVSSetStringF(const char *key, const char *val, void *arg, int errnum));
typedef void (KVSSetIntF(const char *key, int val, void *arg, int errnum));
typedef void (KVSSetInt64F(const char *key, int64_t val, void *arg,int errnum));
typedef void (KVSSetDoubleF(const char *key, double val, void *arg,int errnum));
typedef void (KVSSetBooleanF(const char *key, bool val, void *arg, int errnum));

/* Destroy a kvsdir object returned from kvs_get_dir() or kvsdir_get_dir()
 */
void kvsdir_destroy (kvsdir_t dir);

/* The basic get and put operations, with convenience functions
 * for simple types.  You will get an error if you call kvs_get()
 * on a directory (return -1, errno = EISDIR).  Use kvs_get_dir() which
 * returns the opaque kvsdir_t type.  kvs_get(), kvs_get_dir(), and
 * kvs_get_string() return values that must be freed with json_object_put(),
 * kvsdir_destroy(), and free() respectively.
 * These functions return -1 on error (errno set), 0 on success.
 */
int kvs_get (void *h, const char *key, json_object **valp);
int kvs_get_dir (void *h, kvsdir_t *dirp, const char *fmt, ...);
int kvs_get_string (void *h, const char *key, char **valp);
int kvs_get_int (void *h, const char *key, int *valp);
int kvs_get_int64 (void *h, const char *key, int64_t *valp);
int kvs_get_double (void *h, const char *key, double *valp);
int kvs_get_boolean (void *h, const char *key, bool *valp);
int kvs_get_symlink (void *h, const char *key, char **valp);

/* kvs_watch* is like kvs_get* except the registered callback is called
 * to set the value.  It will be called immediately to set the initial
 * value and again each time the value changes.  There is currently no
 * "unwatch" function.  Any storage associated with the value given the
 * callback is freed when the callback returns.  If a value is unset, the
 * callback gets errnum = ENOENT.
 */
int kvs_watch (void *h, const char *key, KVSSetF *set, void *arg);
int kvs_watch_dir (void *h, KVSSetDirF *set, void *arg, const char *fmt, ...);
int kvs_watch_string (void *h, const char *key, KVSSetStringF *set, void *arg);
int kvs_watch_int (void *h, const char *key, KVSSetIntF *set, void *arg);
int kvs_watch_int64 (void *h, const char *key, KVSSetInt64F *set, void *arg);
int kvs_watch_double (void *h, const char *key, KVSSetDoubleF *set, void *arg);
int kvs_watch_boolean (void *h, const char *key, KVSSetBooleanF *set,void *arg);

/* While the above callback interface makes sense in plugin context,
 * the following is better for API context.  'valp' is an IN/OUT parameter.
 * You should first read the current value, then pass it into the
 * kvs_watch_once call, which will return with a new value when it changes.
 * (The original value is freed inside the function; the new one must
 * be freed by the caller).
 * FIXME: add more types.
 */
int kvs_watch_once (void *h, const char *key, json_object **valp);
int kvs_watch_once_int (void *h, const char *key, int *valp);

/* kvs_put() and kvs_put_string() both make copies of the value argument
 * The caller retains ownership of the original.
 * These functions return -1 on error (errno set), 0 on success.
 */
int kvs_put (void *h, const char *key, json_object *val);
int kvs_put_string (void *h, const char *key, const char *val);
int kvs_put_int (void *h, const char *key, int val);
int kvs_put_int64 (void *h, const char *key, int64_t val);
int kvs_put_double (void *h, const char *key, double val);
int kvs_put_boolean (void *h, const char *key, bool val);

/* An iterator interface for walking the list of names in a kvsdir_t
 * returned by kvs_get_dir().  kvsitr_create() always succeeds.
 * kvsitr_next() returns NULL when the last item is reached.
 */
typedef struct kvsdir_iterator_struct *kvsitr_t;
kvsitr_t kvsitr_create (kvsdir_t dir);
void kvsitr_destroy (kvsitr_t itr);
const char *kvsitr_next (kvsitr_t itr);
void kvsitr_rewind (kvsitr_t itr);

/* Test attributes of 'name', relative to kvsdir object.
 * This is intended for testing names returned by kvsitr_next (no recursion).
 * Symlinks are not dereferenced, i.e. symlink pointing to dir will read
 * issymlink=true, isdir=false.
 */
bool kvsdir_exists (kvsdir_t dir, const char *name);
bool kvsdir_isdir (kvsdir_t dir, const char *name);
bool kvsdir_issymlink (kvsdir_t dir, const char *name);

/* Get key associated with a directory or directory entry.
 * Both functions always succeed.
 */
const char *kvsdir_key (kvsdir_t dir);
char *kvsdir_key_at (kvsdir_t dir, const char *key); /* caller frees result */
void *kvsdir_handle (kvsdir_t dir);

/* Remove a key from the namespace.  If it represents a directory,
 * its contents are also removed.  kvsdir_unlink removes it relative to 'dir'.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_unlink (void *h, const char *key);

/* Create symlink.  kvsdir_symlink creates it relatived to 'dir'.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_symlink (void *h, const char *key, const char *target);

/* Create an empty directory.  kvsdir_mkdir creates it relative to 'dir'.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_mkdir (void *h, const char *key);

/* kvs_commit() must be called after kvs_put*, kvs_unlink, and kvs_mkdir
 * to finalize the update.  The new data is immediately available on
 * the calling node when the commit returns.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_commit (void *h);

/* kvs_fence() is a collective commit operation.  nprocs tasks make the
 * call with identical arguments.  It is internally optimized to minimize
 * the work that needs to be done.  Once the call returns, all changes
 * from participating tasks are available to all tasks.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_fence (void *h, const char *name, int nprocs);

/* Synchronization:
 * Process A commits data, then gets the store version V and sends it to B.
 * Process B waits for the store version to be >= V, then reads data.
 */
int kvs_get_version (void *h, int *versionp);
int kvs_wait_version (void *h, int version);

/* These are called internally by plugin.c and apicli.c and
 * are part of the KVS internal implementation.  Do not use.
 */
void kvs_getctxfun_set (KVSGetCtxF *fun);
void kvs_watch_response (void *h, zmsg_t **zmsg);
kvsctx_t kvs_ctx_create (void *h);
void kvs_ctx_destroy (kvsctx_t ctx);

/* Garbage collect the cache.  On the root node, drop all data that
 * doesn't have a reference in the namespace.  On other nodes, the entire
 * cache is dropped and will be reloaded on demand.
 * Returns -1 on error (errno set), 0 on success.
 */
int kvs_dropcache (void *h);

/* kvsdir_ convenience functions
 * They behave exactly like their kvs_ counterparts, except the 'key' path
 * is resolved relative to the directory.
 */
int kvsdir_get (kvsdir_t dir, const char *key, json_object **valp);
int kvsdir_get_dir (kvsdir_t dir, kvsdir_t *dirp, const char *fmt, ...);
int kvsdir_get_string (kvsdir_t dir, const char *key, char **valp);
int kvsdir_get_int (kvsdir_t dir, const char *key, int *valp);
int kvsdir_get_int64 (kvsdir_t dir, const char *key, int64_t *valp);
int kvsdir_get_double (kvsdir_t dir, const char *key, double *valp);
int kvsdir_get_boolean (kvsdir_t dir, const char *key, bool *valp);
int kvsdir_get_symlink (kvsdir_t dir, const char *key, char **valp);

int kvsdir_put (kvsdir_t dir, const char *key, json_object *val);
int kvsdir_put_string (kvsdir_t dir, const char *key, const char *val);
int kvsdir_put_int (kvsdir_t dir, const char *key, int val);
int kvsdir_put_int64 (kvsdir_t dir, const char *key, int64_t val);
int kvsdir_put_double (kvsdir_t dir, const char *key, double val);
int kvsdir_put_boolean (kvsdir_t dir, const char *key, bool val);

int kvsdir_unlink (kvsdir_t dir, const char *key);
int kvsdir_symlink (kvsdir_t dir, const char *key, const char *target);
int kvsdir_mkdir (kvsdir_t dir, const char *key);


#endif /* !HAVE_KVS_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
