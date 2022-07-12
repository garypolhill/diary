#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>

#define MAGIC_SIZE 4

typedef struct {
  const char *path;
  long long wait_ns;
  int err;
  pthread_t stat_thread;
  pthread_t wait_thread;
  pthread_mutex_t mutex;
  filestat_t *filestat;
} stat_call_t;

filestat_t *file_stat(const char *path, long long nsec, int *err) {
  stat_call_t stat_args;
  void *status;

  stat_args.path = path;
  stat_args.wait_ns = nsec;
  stat_args.err = 0;
  pthread_mutex_lock(&stat_args.mutex);
  pthread_create(&stat_args.wait_thread, NULL, wait_stat_th, &stat_args);
  pthread_create(&stat_args.stat_thread, NULL, stat_th, &stat_args);
  pthread_mutex_unlock(&stat_args.mutex);

  { /* Not sure what the significance is of putting these statements in
     * blocks as in foo_timedwait() at the web address above
     */
    pthread_cleanup_push(pthread_cancel, stat_args.stat_thread);
    pthread_cleanup_push(pthread_detach, stat_args.stat_thread);
    {
      pthread_cleanup_push(pthread_cancel, stat_args.wait_thread);
      pthread_cleanup_push(pthread_detach, stat_args.wait_thread);
      pthread_join(stat_args.wait_thread, &status);
      pthread_cleanup_pop(0);
      pthread_cleanup_pop(0);
    }
    pthread_join(stat_args.stat_thread, &status);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
  }
  (*err) = (stat_args.err == 0 ? (status == PTHREAD_CANCELED ? ETIMEDOUT : 0) : stat_args.err);
  return stat_args->filestat;
}

void *wait_stat_th(void *args) {
  struct timespec ts;
  struct timespec rm;
  long long sec;
  long long nsec;
  stat_call_t *p;

  p = (stat_call_t *)args;

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
    pthread_cancel(p->stat_thread);
    pthread_cleanup_pop(1);
    return NULL;
  }
}

void *stat_th(void *args) {
  stat_call_t *p;

  p = (stat_call_t *)args;
  stat_file(p);
  pthread_mutex_lock(&p->mutex);
  pthread_cleanup_push(pthread_mutex_unlock, &p->mutex);
  pthread_testcancel();
  pthread_cancel(p->wait_thread);
  pthread_cleanup_pop(1);
  return NULL;
}

void stat_file(stat_call_t *p) {
  int ok;

  p->filestat = calloc(1, sizeof(filestat_t));
  if(p->filestat == NULL) {

  }
  p->filestat->path = strdup(p->path);
  if(p->filestat->path == NULL) {

  }
  ok = stat(p->path, &(p->filestat->st));
  if(ok < 0) {
    p->err = errno;
  }
  else {
    struct timespec t[2];
    int fd;

    p->filestat->magic = calloc(MAGIC_SIZE, sizeof(char));
    if(p->filestat->magic == NULL) {

    }

    t[0] = p->filestat->st.st_atimespec;
    t[1] = p->filestat->st.st_mtimespec;

    fd = open(p->path, O_RDONLY);
    if(fd < 0) {

    }
    ok = read(fd, buf, (size_t)MAGIC_SIZE);
    if(ok < 0) {

    }
    close(fd);

    /* reset access and modify times */

    ok = utimensat(0, p->path, t, 0);
    if(ok < 0) {
      
    }
  }
}

void free_filestat_t(filestat_t *p) {
  if(p->path != NULL) {
    free(p->path);
  }
  if(p->md5 != NULL) {
    free(p->md5);
  }
  if(p->magic != NULL) {
    free(p->magic);
  }
  free(p);
}
