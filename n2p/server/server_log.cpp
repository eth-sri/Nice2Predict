/*
 * server_log.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: veselin
 */

#include <glog/logging.h>

#include "base/stringprintf.h"

#include "server_log.h"

Nice2ServerLog::Nice2ServerLog(const std::string& logfile_prefix) {
  time_t tt;
  time(&tt);
  tm* t = gmtime(&tt);

  filename_.clear();
  int attempt = 0;
  while (filename_.empty() || access( filename_.c_str(), F_OK ) != -1) {
    if (attempt > 10)
      LOG(FATAL) << "Attempted logging file exists " << filename_;
    filename_ = StringPrintf("%s-%.4d%.2d%.2d-%.2d.%.2d.%.2d-%d",
        logfile_prefix.c_str(),
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        attempt);
    ++attempt;
  }
  f_ = fopen(filename_.c_str(), "wt");
  if (f_ == NULL) {
    LOG(FATAL) << "Could not create logging file " << filename_;
  }
}

void Nice2ServerLog::LogRecord(const std::string& record) {
  time_t tt;
  time(&tt);
  tm* t = gmtime(&tt);

  // TODO: Put the logging in a separate thread to not block responding.
  std::lock_guard<std::mutex> guard(file_mutex_);
  fprintf(f_, "{ \"time\":\"%.4d%.2d%.2d-%.2d.%.2d.%.2d\", ",
      t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
      t->tm_hour, t->tm_min, t->tm_sec);
  fwrite(record.data(), record.size(), 1, f_);
  fprintf(f_, "}\n");
  fflush(f_);
}

Nice2ServerLog::~Nice2ServerLog() {
  fclose(f_);
}
