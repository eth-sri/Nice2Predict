/*
   Copyright 2014 Software Reliability Lab, ETH Zurich

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#ifndef BASE_READERUTIL_H_
#define BASE_READERUTIL_H_

#include <unistd.h>
#include "glog/logging.h"

#include <mutex>
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>

#include "fileutil.h"

// Readers

class InputRecordReader {
public:
  virtual ~InputRecordReader() {}
  virtual bool ReachedEnd() = 0;
  virtual void Read(std::string* s) = 0;
};

class FileInputRecordReader : public InputRecordReader {
public:
  explicit FileInputRecordReader(const std::string& filename) : file(filename) {
    CHECK(exists(filename)) << "File '" << filename << "' does not exist!";
  }
  virtual ~FileInputRecordReader() override {
    file.close();
  }
  std::ifstream file;
  std::mutex filemutex;

  virtual void Read(std::string* s) override {
    std::lock_guard<std::mutex> lock(filemutex);
    s->clear();
    while (s->empty()) {
      if (file.eof() || !file.good()) {
        return;  // Keep empty.
      }
      std::getline(file, *s);  // Read until we get a non-empty line.
    }
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(filemutex);
    return file.eof();
  }
private:
  inline bool exists (const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
  }
};

class FileListRecordReader : public InputRecordReader {
public:
  explicit FileListRecordReader(const std::vector<std::string>& filelist) : filelist_(filelist), file_index_(0) {
  }
  virtual ~FileListRecordReader() override {
  }

  virtual void Read(std::string* s) override {
    s->clear();

    std::string filename;
    {
      std::lock_guard<std::mutex> lock(file_index_mutex_);
      if (file_index_ >= filelist_.size()) return;
      filename = filelist_[file_index_];
      file_index_++;
    }

    CHECK(exists(filename)) << "File '" << filename << "' does not exist!";
    ReadFileToStringOrDie(filename.c_str(), s);
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(file_index_mutex_);
    return file_index_ >= filelist_.size();
  }

private:
  inline bool exists (const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
  }

  const std::vector<std::string>& filelist_;
  size_t file_index_;
  std::mutex file_index_mutex_;
};

class CachingInputRecordReader : public InputRecordReader {
public:
  // The class takes ownership of underlying_reader.
  explicit CachingInputRecordReader(
      InputRecordReader* underlying_reader,
      std::vector<std::string>* recording) : underlying_reader_(underlying_reader), recording_(recording) {
  }
  virtual ~CachingInputRecordReader() override {
    delete underlying_reader_;
  }

  virtual void Read(std::string* s) override {
    underlying_reader_->Read(s);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!s->empty()) {
      recording_->push_back(*s);
    }
  }

  virtual bool ReachedEnd() override {
    return underlying_reader_->ReachedEnd();
  }

private:
  InputRecordReader* underlying_reader_;
  std::vector<std::string>* recording_;
  std::mutex mutex_;
};

class RecordedRecordReader : public InputRecordReader {
public:
  explicit RecordedRecordReader(const std::vector<std::string>* recording) : recording_(recording), pos_(0) {
  }
  virtual ~RecordedRecordReader() {
  }

  virtual void Read(std::string* s) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pos_ >= recording_->size()) {
      s->clear();
    } else {
      (*s) = (*recording_)[pos_];
      ++pos_;
    }
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return pos_ >= recording_->size();
  }

private:
  const std::vector<std::string>* recording_;
  size_t pos_;
  std::mutex mutex_;
};

// Factories

class RecordInput {
public:
  virtual ~RecordInput() {}
  virtual InputRecordReader* CreateReader() = 0;
};

// Input where each record is a line in a file.
class FileRecordInput : public RecordInput {
public:
  explicit FileRecordInput(const std::string& filename) : filename_(filename) {
  }
  virtual ~FileRecordInput() override {
  }

  virtual InputRecordReader* CreateReader() override {
    return new FileInputRecordReader(filename_);
  }

private:
  std::string filename_;
};

// Input where each records is the contents of a file.
class FileListRecordInput : public RecordInput {
public:
  explicit FileListRecordInput(std::vector<std::string>&& files) : files_(files) {
  }
  virtual ~FileListRecordInput() override {
  }

  virtual InputRecordReader* CreateReader() override {
    return new FileListRecordReader(files_);
  }

private:
  std::vector<std::string> files_;
};

/**
 * Input for which the first created reader reads the records from a file and then remembers them in RAM.
 * Each subsequent reader gets the cached records (file lines), but in randomly shuffled order.
 * Concurrency: Once a reader is created, multiple threads can read from it. However, only one
 * reader should be created at a time.
 */
class ShuffledCacheInput : public RecordInput {
public:
  // The class takes ownership of underlying_input.
  explicit ShuffledCacheInput(RecordInput* underlying_input) : underlying_input_(underlying_input), has_recorded_(false) {
  }
  virtual ~ShuffledCacheInput() override {
    delete underlying_input_;
  }

  virtual InputRecordReader* CreateReader() override {
    if (!has_recorded_) {
      has_recorded_ = true;
      return new CachingInputRecordReader(underlying_input_->CreateReader(), &recorded_cache_);
    }

    std::random_shuffle(recorded_cache_.begin(), recorded_cache_.end());
    return new RecordedRecordReader(&recorded_cache_);
  }

private:
  RecordInput* underlying_input_;
  bool has_recorded_;
  std::vector<std::string> recorded_cache_;
};


// Cross validation.

class CrossValidationReader : public InputRecordReader {
public:
  explicit CrossValidationReader(
      InputRecordReader* underlying_reader,
      int fold_id,
      int num_folds,
      bool training)
      : underlying_reader_(underlying_reader),
        fold_id_(fold_id),
        num_folds_(num_folds),
        training_(training),
        row_id_(0) {
  }
  virtual ~CrossValidationReader() override {
    delete underlying_reader_;
  }

  virtual void Read(std::string* s) override {
    std::lock_guard<std::mutex> lock(mutex_);
    for (;;) {
      ++row_id_;
      if ((training_ && (row_id_ % num_folds_) != fold_id_) ||
          (!training_ && (row_id_ % num_folds_) == fold_id_)) {
        underlying_reader_->Read(s);
        break;
      } else {
        std::string tmp;
        underlying_reader_->Read(&tmp);
      }
    }
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return underlying_reader_->ReachedEnd();
  }

private:
  InputRecordReader* underlying_reader_;
  std::mutex mutex_;
  int fold_id_;
  int num_folds_;
  bool training_;
  int row_id_;
};

class CrossValidationInput : public RecordInput {
public:
  CrossValidationInput(
      RecordInput* underlying_input,
      int fold_id,
      int num_folds,
      bool training) : underlying_input_(underlying_input), fold_id_(fold_id), num_folds_(num_folds), training_(training) {
  }
  ~CrossValidationInput() {
    delete underlying_input_;
  }

  virtual InputRecordReader* CreateReader() override {
    return new CrossValidationReader(
        underlying_input_->CreateReader(), fold_id_, num_folds_, training_);
  }

private:
  RecordInput* underlying_input_;
  int fold_id_;
  int num_folds_;
  bool training_;
};

#endif /* BASE_READERUTIL_H_ */
