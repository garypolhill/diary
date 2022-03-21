/* ls.c
 *
 * Program to list the contents of a directory using threads and to get
 * impatient if an answer is not returned.
 *
 * The implementation is based on code at
 * http://www.mit.edu/people/proven/IAP_2000/timedwait.html
 *
 * Gary Polhill 20 March 2022
 */

#include "ls.h"

/* "Private" data types */

typedef struct {
  const char *path;
  dirlist_t *list;
  long long wait_ns;
  int err;
  DIR *dp;
  pthread_t ls_thread;
  pthread_t wait_thread;
  pthread_mutex_t mutex;
} ls_call_t;

/* "Private" function declarations */

void *ls_th(void *args);
void *wait_th(void *args);
void ls_dir(ls_call_t *p);
dirarray_t *alloc_dirarray_free_dirlist(dirlist_t *list, int err);

/* Optional main()
 */

#ifdef COMPILE_LS_MAIN
int main(int argc, const char * const *argv) {
  if(argc <= 1) {
    fprintf(stderr, "Usage: %s <path>\n", argc == 0 ? "$TLS" : argv[0]);
    exit(1);
  }
  else {
    dirarray_t *list;

    list = lsdft(argv[1]);
    if(list == NULL) {
      fprintf(stderr, "NULL returned from lsdft(%s)\n", argv[1]);
      exit(1);
    }
    if(list->err == 0) {
      int i;

      for(i = 0; i < list->n; i++) {
        const char *type;

        switch(list->entries[i]->d_type) {
#ifdef DT_UNKNOWN
        case DT_UNKNOWN:
          type = "unknown filetype";
          break;
#endif
#ifdef DT_FIFO
        case DT_FIFO:
          type = "named pipe";
          break;
#endif
#ifdef DT_CHR
        case DT_CHR:
          type = "character device";
          break;
#endif
#ifdef DT_DIR
        case DT_DIR:
          type = "directory";
          break;
#endif
#ifdef DT_BLK
        case DT_BLK:
          type = "block device";
          break;
#endif
#ifdef DT_REG
        case DT_REG:
          type = "regular file";
          break;
#endif
#ifdef DT_LNK
        case DT_LNK:
          type = "symbolic link";
          break;
#endif
#ifdef DT_SOCK
        case DT_SOCK:
          type = "socket";
          break;
#endif
#ifdef DT_WHT
        case DT_WHT:
          type = "dummy whiteout inode";
          break;
#endif
        default:
          type = "unrecognized filetype";
        }

        printf("%s file \"%s\" has size %llu\n", type, list->entries[i]->d_name,
          (unsigned long long)list->entries[i]->d_reclen);
      }
      free_dirarray(list);
      exit(0);
    }
    else {
      fprintf(stderr, "lsdft(%s) generated error: %s\n", argv[1], strerror(list->err));
      free_dirarray(list);
      exit(1);
    }

  }
}
#endif

/*
 * Function definitions
 *****************************************************************************/

dirarray_t *lsdft(const char *path) {
  return ls(path, LS_DEFAULT_NSEC);
}

/* ls()
 *
 * Threaded ls function
 */

dirarray_t *ls(const char *path, long long nsec) {
  dirlist_t *list;
  int err;

  list = ls_list(path, nsec, &err);

  return alloc_dirarray_free_dirlist(list, err);
}

dirlist_t *ls_list(const char *path, long long nsec, int *err) {
  ls_call_t ls_args;
  void *status;

  ls_args.path = path;
  ls_args.list = NULL;
  ls_args.wait_ns = nsec;
  ls_args.err = 0;
  ls_args.dp = NULL;
  pthread_mutex_lock(&ls_args.mutex);
  pthread_create(&ls_args.wait_thread, NULL, wait_th, &ls_args);
  pthread_create(&ls_args.ls_thread, NULL, ls_th, &ls_args);
  pthread_mutex_unlock(&ls_args.mutex);

  { /* Not sure what the significance is of putting these statements in
     * blocks as in foo_timedwait() at the web address above
     */
    pthread_cleanup_push(pthread_cancel, ls_args.ls_thread);
    pthread_cleanup_push(pthread_detach, ls_args.ls_thread);
    {
      pthread_cleanup_push(pthread_cancel, ls_args.wait_thread);
      pthread_cleanup_push(pthread_detach, ls_args.wait_thread);
      pthread_join(ls_args.wait_thread, &status);
      pthread_cleanup_pop(0);
      pthread_cleanup_pop(0);
    }
    pthread_join(ls_args.ls_thread, &status);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
  }
  (*err) = (ls_args.err == 0 ? (status == PTHREAD_CANCELED ? ETIMEDOUT : 0) : ls_args.err);
  if(ls_args.dp != NULL) closedir(ls_args.dp);
  return ls_args.list;
}

void *wait_th(void *args) {
  struct timespec ts;
  struct timespec rm;
  long long sec;
  long long nsec;
  ls_call_t *p;

  p = (ls_call_t *)args;

  sec = p->wait_ns / 1000000000LL;
  nsec = p->wait_ns - (sec * 1000000000LL);
  ts.tv_sec = (time_t)sec;
  ts.tv_nsec = (long)nsec;
  if(nanosleep(&ts, &rm) != 0) {
    /* Interrupted (EINTR) or invalid call (EFAULT or EINVAL) */
    perror("nanosleep");
  } else {
    /* We slept for the required time, kill off the delayed thread! */
    pthread_mutex_lock(&p->mutex);
    pthread_cleanup_push(pthread_mutex_unlock, &p->mutex);
    pthread_testcancel();
    pthread_cancel(p->ls_thread);
    pthread_cleanup_pop(1);
    return NULL;
  }
}

void *ls_th(void *args) {
  ls_call_t *p;

  p = (ls_call_t *)args;
  ls_dir(p);
  pthread_mutex_lock(&p->mutex);
  pthread_cleanup_push(pthread_mutex_unlock, &p->mutex);
  pthread_testcancel();
  pthread_cancel(p->wait_thread);
  pthread_cleanup_pop(1);
  return NULL;
}

void ls_dir(ls_call_t *p) {
  struct dirent *entry;

  p->dp = opendir(p->path);
  p->list = NULL;

  if(p->dp == NULL) {
    p->err = errno;
    return;
  }

  while( (entry = readdir(p->dp)) != NULL ) {
    dirlist_t *result;

    result = alloc_dirlist(entry, p->list);

    if(result != NULL) {
      p->list = result;
    }
    else {
      if(p->list != NULL) p->err = p->list->err;
      return;
    }
  }

  closedir(p->dp);
  p->dp = NULL;
}

dirlist_t *alloc_dirlist(struct dirent *ent, dirlist_t *tl) {
  dirlist_t *p;

  p = (dirlist_t *)malloc(sizeof(dirlist_t));
  if(p != NULL) {
    p->entry = (struct dirent *)malloc(sizeof(struct dirent));
    if(p->entry == NULL) {
      tl->err = errno;
      free(p);
      return NULL;
    }
    memcpy(p->entry, ent, sizeof(struct dirent));
    p->next = tl;
    p->err = 0;
    return p;
  }
  else {
    tl->err = errno;
    return NULL;
  }

}

dirarray_t *alloc_dirarray_free_dirlist(dirlist_t *list, int err) {
  int n = 0;
  dirlist_t *p;
  dirarray_t *r;

  for(p = list; p != NULL; p = p->next) {
    n++;
    if(err == 0 && p->err != 0) {
      err = p->err;
    }
  }

  if(n == 0 && err == 0) {
    return NULL;
  }
  else if(err != 0) {
    r = (dirarray_t *)malloc(sizeof(dirarray_t));
    if(r == NULL) return NULL;
    r->entries = NULL;
    r->n = 0;
    r->err = err;
    free_dirlist(list);
  }
  else {
    r = (dirarray_t *)malloc(sizeof(dirarray_t));
    if(r == NULL) return NULL;
    r->entries = (struct dirent **)calloc(n, sizeof(struct dirent *));
    if(r->entries == NULL) {
      r->n = 0;
      r->err = errno;
      free_dirlist(list);
    }
    else {
      dirlist_t *q = NULL;
      r->n = n;
      r->err = 0;
      for(p = list; p != NULL; p = p->next) {
        if(q != NULL) free(q);

        n--;
        if(n >= 0) {
          r->entries[n] = p->entry;
        }
        else {
          fprintf(stderr, "BUG! (%d)", n);
        }

        q = p;
      }
      if(q != NULL) free(q);
    }
  }
  return r;
}

void free_dirlist(dirlist_t *p) {
  dirlist_t *q = NULL;

  for(; p != NULL; p = p->next) {
    if(q != NULL) free(q);
    free(p->entry);
    q = p;
  }
  if(q != NULL) free(q);
}

void free_dirarray(dirarray_t *p) {
  if(p != NULL) {
    if(p->entries != NULL) {
      int i;

      for(i = 0; i < p->n; i++) {
        free(p->entries[i]);
      }
      free(p->entries);
    }
    free(p);
  }
}
