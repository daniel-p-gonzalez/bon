/*----------------------------------------------------------------------------*\
|*
|* Support functions for stdlib
|*
L*----------------------------------------------------------------------------*/
#include <iostream>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <cstring>

extern "C" void print_string(char* str) {
  std::cout << str << std::endl;
  return;
}

extern "C" void write_string(char* str) {
  std::cout << str;
  return;
}

extern "C" char* str_concat(char* str1, char* str2) {
  std::string result = std::string(str1) + std::string(str2);
  char* new_str = new char[result.size()+1];
  size_t idx = 0;
  for (auto &chr : result) {
    new_str[idx++] = chr;
  }
  new_str[result.size()] = 0;
  return new_str;
}

extern "C" bool str_eq(char* str1, char* str2) {
  return strcmp(str1, str2) == 0;
}

extern "C" char* int_to_string(int64_t val) {
  std::ostringstream val_stream;
  val_stream << val;
  std::string result = val_stream.str();
  char* new_str = new char[result.size()+1];
  size_t idx = 0;
  for (auto &chr : result) {
    new_str[idx++] = chr;
  }
  new_str[result.size()] = 0;
  return new_str;
}

extern "C" char* float_to_string(double val) {
  std::ostringstream val_stream;
  val_stream.precision(15);
  val_stream << val;
  std::string result = val_stream.str();
  char* new_str = new char[result.size()+1];
  size_t idx = 0;
  for (auto &chr : result) {
    new_str[idx++] = chr;
  }
  new_str[result.size()] = 0;
  return new_str;
}
