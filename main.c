#include <stdio.h>  // Input/output library
#include <signal.h> // Signal handling library
#include "sh.h"


void sig_handler(int signal);

int main( int argc, char **argv, char **envp ) {
  /* put signal set up stuff here */
  signal(SIGINT, SIG_IGN);  // Ignore Ctrl-C
  signal(SIGTSTP, SIG_IGN); // Ignore Ctrl-Z
  signal(SIGTERM, SIG_IGN); // Ignore SIGTERM

  // signal(SIGINT, sig_handler);
  return sh(argc, argv, envp);
}

void sig_handler(int signal) {
  /* define your signal handler */
  if (signal == SIGINT) {
    printf("\nUse 'exit' to quit.\n");
    fflush(stdout);
  }
}