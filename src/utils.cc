/*----------------------------------------------------------------------------*\
|*
|* Every project has a "utils" for functions that need a home. This is Bon's.
|*
L*----------------------------------------------------------------------------*/

#include <cstdio>
#include <iostream>
#include "utils.h"
#include "bonLogger.h"

std::string BON_STDLIB_PATH;

bool file_to_stdin(std::string filename) {
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
