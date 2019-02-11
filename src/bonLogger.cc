/*----------------------------------------------------------------------------*\
|*
|* Simple logger for communicating compiler warnings and errors
|*
L*----------------------------------------------------------------------------*/

#include "bonLogger.h"
#include <iostream>
#include <fstream>

namespace bon {

Logger logger;

Logger::Logger(uint32_t max_errors, uint32_t max_warnings,
               bool always_display_final_msg)
: current_file_(""), line_num_(0), column_num_(0),
  max_errors_(max_errors), max_warnings_(max_warnings),
  warn_count_(0), error_count_(0),
  always_display_final_msg_(always_display_final_msg)
{
}

bool Logger::had_errors() {
    return error_count_ > 0 || warn_count_ >= max_warnings_;
}

void Logger::set_current_file(std::string current_file) {
    current_file_ = current_file;
}

std::string Logger::get_current_file() {
    return current_file_;
}

void Logger::set_line_column(DocPosition pos) {
    line_num_ = pos.line;
    column_num_ = pos.column;
}

void Logger::set_line_column(size_t line_num, size_t column_num) {
    line_num_ = line_num;
    column_num_ = column_num;
}

void Logger::config(uint32_t max_errors, uint32_t max_warnings,
                    bool always_display_final_msg) {
    max_errors_= max_errors;
    max_warnings_= max_warnings;
    always_display_final_msg_ = always_display_final_msg;
}

void Logger::finalize() {
    if (had_errors()) {
        std::cout << "FAILED with " << warn_count_ << " warnings and "
                  << error_count_ << " errors." << std::endl;
    }
    else if (warn_count_ > 0 || always_display_final_msg_) {
        std::cout << "Succeeded with " << warn_count_ << " warnings and "
                  << error_count_ << " errors." << std::endl;
    }
}

void Logger::info(std::string type, std::ostream &message_stream) {
    std::string message =
        dynamic_cast<std::ostringstream&>(message_stream).str();
    info(type, message);
}

void Logger::info(std::string type, std::string message) {
    const std::string grey = "\x1b[90m";
    output_message(type, grey, message);
}

void Logger::warn(std::string type, std::ostream &message_stream) {
    std::string message =
        dynamic_cast<std::ostringstream&>(message_stream).str();
    warn(type, message);
}

void Logger::warn(std::string type, std::string message) {
    ++warn_count_;

    const std::string magenta = "\x1b[35m";
    output_message(type, magenta, message);

    if (warn_count_ >= max_warnings_) {
        throw max_warnings_exception();
    }
}

void Logger::error(std::string type, std::ostream &message_stream) {
    std::string message =
        dynamic_cast<std::ostringstream&>(message_stream).str();
    error(type, message);
}

void Logger::error(std::string type, std::string message) {
    ++error_count_;

    const std::string red = "\x1b[31m";
    output_message(type, red, message);

    if (error_count_ >= max_errors_) {
        throw max_errors_exception();
    }
}

std::string Logger::get_context() {
    // TODO: this is obviously not good.
    //       need to buffer lines in lexer, and set them in logger
    std::ifstream fin(current_file_);
    size_t i = 0;
    for (std::string line; getline(fin, line); ) {
        ++i;
        if (i >= line_num_) {
            return line;
        }
    }
    return "";
}

void Logger::output_message(std::string type, std::string type_color,
                            std::string message) {
    const std::string bold = "\x1b[1m";
    const std::string white = "\x1b[97m";
    const std::string endclr = "\x1b[0m";
    std::cout << bold << white << current_file_ << ":" << line_num_ << ":"
              << column_num_+1 << ": " << endclr << endclr;
    std::cout << bold << type_color << type << ": " << endclr << endclr;
    std::cout << bold << white << message << endclr << endclr << std::endl;
    // std::cout << "In context:" << std::endl;

    std::cout << get_context() << std::endl;

    for (size_t i = 0; i < column_num_; ++i) {
        std::cout << " ";
    }
    const std::string green = "\x1b[32m";
    std::cout << bold << green << "^" << endclr << endclr << std::endl;
}

} // namespace bon
