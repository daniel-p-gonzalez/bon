/*----------------------------------------------------------------------------*\
|*
|* Every project has a "utils" for functions that need a home. This is Bon's.
|*
L*----------------------------------------------------------------------------*/

#include <cstdio>
#include <unistd.h>
#include <iostream>
#include "utils.h"
#include "bonLogger.h"

std::string BON_STDLIB_PATH;
int stdin_orig = -1;

bool file_to_stdin(std::string filename) {
  if (stdin_orig == -1) {
    stdin_orig = dup(0);
  }
  fflush(stdin);
  if (!freopen(filename.c_str(), "r", stdin)) {
    // try again within stdlib directory
    fflush(stdin);
    if (!freopen((BON_STDLIB_PATH + "/" + filename).c_str(), "r", stdin)) {
      bon::logger.set_line_column(0, 0);
      bon::logger.error("error", "File not found: " + filename);
      return false;
    }
  }
  return true;
}

void restore_stdin() {
  fflush(stdin);
  fclose(stdin);
  *stdin = *fdopen(stdin_orig, "r");
  // close(stdin_orig);
  // fflush(stdin);
}
