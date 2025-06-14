/*
  Just a little sample function that gets the PATH env var, parses it and
  puts "components" into a linked list, which is returned.
*/
#include "get_path.h"

struct pathelement *get_path(char **path_ptr) {
  char *p;
  struct pathelement *tmp, *pathlist = NULL;

  p = getenv("PATH");    /* get a pointer to the PATH env var.
                            make a copy of it, since strtok modifies the
                            string that it is working with... */
  *path_ptr = malloc((strlen(p) + 1) * sizeof(char));  // Allocate memory
  strncpy(*path_ptr, p, strlen(p));
  (*path_ptr)[strlen(p)] = '\0';

  p = strtok(*path_ptr, ":");   /* PATH is : delimited */
  do {                          /* loop through the PATH to build a linked list of dirs */
    if (!pathlist) {            /* create head of list */
      tmp = calloc(1, sizeof(struct pathelement));
      pathlist = tmp;
    } else {                    /* add on next element */
      tmp->next = calloc(1, sizeof(struct pathelement));
      tmp = tmp->next;
    }
    tmp->element = p;
    tmp->next = NULL;
  } while ((p = strtok(NULL, ":")));

  return pathlist;
}