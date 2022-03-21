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
