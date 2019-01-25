/*----------------------------------------------------------------------------*\
|*
|* Simple logger for communicating compiler warnings and errors
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include <cstdint>
#include <sstream>
#include <exception>

namespace bon {

class max_warnings_exception : public std::exception {};
class max_errors_exception : public std::exception {};

struct DocPosition
{
  size_t line;
  size_t column;
  DocPosition() : line(0), column(0) {}
  DocPosition(size_t line, size_t column) : line(line), column(column) {}
};

class Logger {
private:
  std::string current_file_;
  size_t line_num_;
  size_t column_num_;
  uint32_t max_errors_;
  uint32_t max_warnings_;
  uint32_t warn_count_;
  uint32_t error_count_;
  bool always_display_final_msg_;

public:
  Logger(uint32_t max_errors=20, uint32_t max_warnings=100,
         bool always_display_final_msg=false);

  bool had_errors();
  void set_current_file(std::string current_file);
  std::string get_current_file();
  void set_line_column(DocPosition pos);
  void set_line_column(size_t line_num, size_t column_num);
  void config(uint32_t max_errors=20, uint32_t max_warnings=100,
              bool always_display_final_msg=false);
  void finalize();

  // returns current line
  std::string get_context();

  void info(std::string type, std::ostream &message_stream);
  void info(std::string type, std::string message);

  void warn(std::string type, std::ostream &message_stream);
  void warn(std::string type, std::string message);

  void error(std::string type, std::ostream &message_stream);
  void error(std::string type, std::string message);

private:
  void output_message(std::string type, std::string type_color,
                      std::string message);
};

extern Logger logger;

} // namespace bon
