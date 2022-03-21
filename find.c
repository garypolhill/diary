/* find.c
 *
 * Implementation of the impatient find function
 *
 * Gary Polhill 21 March 2022
 */

#include "find.h"

#ifdef COMPILE_FIND_MAIN

int main(int argc, const char * const *argv) {
  filefifo_t *queue;
  filelist_t *timeouts;
  filelist_t *errs;
  pathlist_t *list;

  list = find_path(argv[1], LS_DEFAULT_NSEC, &queue, &timeouts, &errs);
}

#endif

pathlist_t *find_path(const char *path, long long nsec, filefifo_t **queue,
                      filelist_t **timeouts, filelist_t **errs) {
  dirlist_t *list;
  dirlist_t *p;
  pathlist_t *r = NULL;
  int err = 0;

  list = ls_list(path, nsec, &err);

  if(list == NULL || err != 0) {
    free_dirlist(list);
    if(err == ETIMEDOUT) {
      (*timeouts) = new_flist(path, NULL, err, (*timeouts));
    }
    else {
      (*errs) = new_flist(path, NULL, err, (*errs));
    }
    return NULL;
  }

  r = new_plist(path, NULL, 0);

  for(p = list; p != NULL; p = p->next) {
    switch(p->entry->d_type) {
#ifdef DT_DIR
    case DT_DIR:
      if((*queue) == NULL) (*queue) = new_fifo();
      push_fifo(path, p->entry, (*queue));
      break;
#endif
#ifdef DT_REG
    case DT_REG:
      push_plist(r, p);
      break;
#endif
#ifdef DT_LNK
    case DT_LNK:
      break;
#endif
    default:
      /* do nothing */
    }
  }

  free_dirlist(list);
  return r;
}

pathlist_t *find_next(long long nsec, filefifo_t **queue,
                      pathlist_t **timeouts, pathlist_t **errs) {
  char *path;
  filelist_t *next;
  pathlist_t *r;

  next = pop_fifo( (*queue) );
  if(next == NULL) return NULL;

  path = (char *)calloc(1 + strlen(next->path) + strlen(next->entry->d_name));
  if(path == NULL) {
    perror("Memory allocation");
    exit(1);
  }
  strcpy(path, next->path);
  strcat(path, "/");
  strcat(path, next->entry->d_name);

  free_flist(next);
  r = find_path(path, nsec, queue, timeouts, errs);
  free(path);
  return r;
}

pathlist_t *new_plist(const char *path, dirlist_t *list) {
  pathlist_t *r;

  r = (pathlist_t *)malloc(sizeof(pathlist_t));
  if(r == NULL) {
    perror("Memory allocation");
    exit(1);
  }
  r->path = strdup(path);
  if(r->path == NULL) {
    perror("Memory allocation");
    exit(1);
  }
  r->list = list;
  return r;
}

void push_plist(pathlist_t *flist, dirlist_t *dlist) {
  flist->list = alloc_dirlist(dlist->entry, flist->list);
}

filefifo_t *new_fifo(void) {
  filefifo_t *fifo;

  fifo = (filefifo_t *)malloc(sizeof(filefifo_t));

  if(fifo == NULL) {
    perror("Memory allocation");
    exit(1);
  }

  return fifo;
}

void push_fifo(const char *path, struct dirent *p, filefifo_t *fifo) {
  filelist_t *list;

  if(p == NULL) return;

  if(fifo == NULL) {
    fprintf(stderr, "BUG! (fifo NULL)\n");
    exit(1);
  }

  list = new_flist(path, p, 0, NULL);

  if(fifo->first == NULL && fifo->last == NULL) {
    fifo->first = list;
  }
  else if(fifo->first == NULL || fifo->last == NULL) {
    fprintf(stderr, "BUG! (fifo->last or fifo->first NULL)\n");
    exit(1);
  }
  else {
    fifo->last->next = list;
  }
  fifo->last = list;

}

filelist_t *pop_fifo(filefifo_t *fifo) {
  filelist_t *r;

  if(fifo == NULL) {
    return NULL;
  }

  r = fifo->first;
  if(fifo->first != NULL) {
    fifo->first = fifo->first->next;
    if(fifo->first == NULL) {
      fifo->last = NULL;
    }
  }

  if(r != NULL) r->next = NULL;
  return r;
}

filelist_t *new_flist(char *path, struct dirent *entry, int err, filelist_t *tail) {
  filelist_t *r;

  r = (filelist_t *)malloc(sizeof(filelist_t));
  if(r == NULL) {
    perror("Memory allocation");
    exit(1);
  }
  r->path = strdup(path);
  if(r->path == NULL) {
    perror("Memory allocation");
    exit(1);
  }
  if(entry != NULL) {
    r->entry = (struct dirent *)malloc(sizeof(struct dirent));
    if(r->entry == NULL) {
      perror("Memory allocation");
      exit(1);
    }
    memcpy(r->entry, entry, sizeof(struct dirent));
  }
  r->err = err;
  return r;
}

void free_flist(filelist_t *list) {
  filelist_t *q = list;

  while(q != NULL) {
    q = q->next;
    free(list->path);
    if(list->entry != NULL) free(list->entry);
    free(list);
    list = q;
  }
}
