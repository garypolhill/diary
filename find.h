/* find.h
 *
 * Header file for the impatient find command implementation
 *
 * Gary Polhill 21 March 2022
 */

#include "ls.h"

typedef struct filelist {
  char *path;
  int err;
  struct dirent *entry;
  struct filelist *next;
} filelist_t;

typedef struct {
  filelist_t *first;
  filelist_t *last;
} filefifo_t;

typedef struct {
  char *path;
  dirlist_t *list;
} pathlist_t;

pathlist_t *find_path(const char *path, long long nsec, filefifo_t **queue,
                      filelist_t **timeouts, filelist_t **errs);
pathlist_t *find_next(long long nsec, filefifo_t **queue,
                      filelist_t **timeouts, filelist_t **errs);
void free_flist(filelist_t *list);
