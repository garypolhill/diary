/* ls.h
 *
 * Gary Polhill 20 March 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define LS_DEFAULT_NSEC 100000000LL
                        /* 0.1s */
typedef struct dirarray {
  struct dirent **entries;
  int n;
  int err;
} dirarray_t;

typedef struct dirlist {
  struct dirent *entry;
  struct dirlist *next;
  int err;
} dirlist_t;

dirarray_t *lsdft(const char *path);
dirarray_t *ls(const char *path, long long nsec);
dirlist_t *ls_list(const char *path, long long nsec, int *err);
dirlist_t *alloc_dirlist(struct dirent *ent, dirlist_t *tl);
void free_dirlist(dirlist_t *p);
void free_dirarray(dirarray_t *p);
