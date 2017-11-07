/*
 * server_log.h
 *
 *  Created on: Jan 28, 2015
 *      Author: veselin
 */

#ifndef SERVER_SERVER_LOG_H_
#define SERVER_SERVER_LOG_H_

#include <mutex>
#include <string>

#include <gflags/gflags.h>

// Implements logging for the server requests/responses.
class Nice2ServerLog {
public:
  Nice2ServerLog(const std::string& logfile_prefix);
  virtual ~Nice2ServerLog();

  void LogRecord(const std::string& record);

private:
  FILE* f_;
  std::string filename_;
  std::mutex file_mutex_;
};

#endif /* SERVER_SERVER_LOG_H_ */
