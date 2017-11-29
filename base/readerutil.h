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

#include <mutex>
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>

#include <glog/logging.h>

#include "fileutil.h"
#include "util/recordio/recordio.h"

// Readers

template <class RecordType>
class InputRecordReader {
public:
  virtual ~InputRecordReader() {}
  virtual bool ReachedEnd() = 0;
  virtual bool Read(RecordType* s) = 0;
};

template <class ProtoClass>
class FileInputRecordReader : public InputRecordReader<ProtoClass> {
 public:
  explicit FileInputRecordReader(const std::string& filename, const int64 max_records=-1) :
      reader(filename),
      has_prefetched(false),
      max_records_(max_records){
  }
  virtual ~FileInputRecordReader() override {
    reader.Close();
  }

  virtual bool Read(ProtoClass* proto) override {
    std::lock_guard<std::mutex> lock(reader_mutex);
    if (!has_prefetched && !PrefetchProto()) {
      proto->Clear();
      return false;
    }
    *proto = std::move(prefetched_proto);
    has_prefetched = false;
    if (max_records_ != 0) {
      max_records_--;
      return true;
    }
    return false;
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(reader_mutex);
    return (!has_prefetched && !PrefetchProto()) || max_records_ == 0;
  }

 private:

  bool PrefetchProto() {
    if (has_prefetched) {
      return true;
    }
    reader.ReadMayNotParse(&prefetched_proto, &has_prefetched);
    return has_prefetched;
  }

  RecordReader reader;
  ProtoClass prefetched_proto;
  bool has_prefetched;
  std::mutex reader_mutex;
  int64 max_records_;
};


template <>
class FileInputRecordReader<std::string> : public InputRecordReader<std::string> {
 public:
  explicit FileInputRecordReader(const std::string& filename, const int64 max_records=-1) :
      file(filename),
      max_records_(max_records) {
    CHECK(exists(filename)) << "File '" << filename << "' does not exist!";
  }
  virtual ~FileInputRecordReader() override {
    file.close();
  }
  std::ifstream file;
  std::mutex filemutex;

  virtual bool Read(std::string* s) override {
    std::lock_guard<std::mutex> lock(filemutex);
    s->clear();
    while (s->empty()) {
      if (file.eof() || !file.good()) {
        return false;  // Keep empty.
      }
      std::getline(file, *s);  // Read until we get a non-empty line.
    }
    if (max_records_ != 0) {
      max_records_--;
      return true;
    }
    return false;
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(filemutex);
    return file.eof() || max_records_ == 0;
  }
 private:
  inline bool exists (const std::string& name) {
    return ( access( name.c_str(), F_OK ) != -1 );
  }

  int64 max_records_;
};

template <class T>
class CachingInputRecordReader : public InputRecordReader<T> {
public:
  // The class takes ownership of underlying_reader.
  explicit CachingInputRecordReader(
      InputRecordReader<T>* underlying_reader,
      std::vector<T>* recording) : underlying_reader_(underlying_reader), recording_(recording) {
  }
  virtual ~CachingInputRecordReader() override {
    delete underlying_reader_;
  }

  virtual bool Read(T* s) override {
    if (!underlying_reader_->Read(s)) {
      return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    recording_->push_back(*s);
    return true;
  }

  virtual bool ReachedEnd() override {
    return underlying_reader_->ReachedEnd();
  }

private:
  InputRecordReader<T>* underlying_reader_;
  std::vector<T>* recording_;
  std::mutex mutex_;
};

template <class T>
class RecordedRecordReader : public InputRecordReader<T> {
public:
  explicit RecordedRecordReader(const std::vector<T>* recording) : recording_(recording), pos_(0) {
  }
  virtual ~RecordedRecordReader() {
  }

  virtual bool Read(T* s) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pos_ >= recording_->size()) {
      return false;
    }
    (*s) = (*recording_)[pos_];
    ++pos_;
    return true;
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return pos_ >= recording_->size();
  }

private:
  const std::vector<T>* recording_;
  size_t pos_;
  std::mutex mutex_;
};

// Factories

template <class T>
class RecordInput {
public:
  virtual ~RecordInput() {}
  virtual InputRecordReader<T>* CreateReader() = 0;
};

template <class T>
class FileRecordInput : public RecordInput<T> {
public:
  explicit FileRecordInput(const std::string& filename, const int64 max_records=-1) :
      filename_(filename),
      max_records_(max_records) {
  }
  virtual ~FileRecordInput() override {
  }

  virtual InputRecordReader<T>* CreateReader() override {
    return new FileInputRecordReader<T>(filename_, max_records_);
  }

private:
  std::string filename_;
  int64 max_records_;
};

/**
 * Input for which the first created reader reads the records from a file and then remembers them in RAM.
 * Each subsequent reader gets the cached records (file lines), but in randomly shuffled order.
 * Concurrency: Once a reader is created, multiple threads can read from it. However, only one
 * reader should be created at a time.
 */
template <class T>
class ShuffledCacheInput : public RecordInput<T> {
public:
  // The class takes ownership of underlying_input.
  explicit ShuffledCacheInput(RecordInput<T>* underlying_input) : underlying_input_(underlying_input), has_recorded_(false) {
  }
  virtual ~ShuffledCacheInput() override {
    delete underlying_input_;
  }

  virtual InputRecordReader<T>* CreateReader() override {
    if (!has_recorded_) {
      has_recorded_ = true;
      return new CachingInputRecordReader<T>(underlying_input_->CreateReader(), &recorded_cache_);
    }

    std::random_shuffle(recorded_cache_.begin(), recorded_cache_.end());
    return new RecordedRecordReader<T>(&recorded_cache_);
  }

private:
  RecordInput<T>* underlying_input_;
  bool has_recorded_;
  std::vector<T> recorded_cache_;
};


// Cross validation.

template <class T>
class CrossValidationReader : public InputRecordReader<T> {
public:
  explicit CrossValidationReader(
      InputRecordReader<T>* underlying_reader,
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

  virtual bool Read(T* s) override {
    std::lock_guard<std::mutex> lock(mutex_);
    for (;;) {
      ++row_id_;
      if ((training_ && (row_id_ % num_folds_) != fold_id_) ||
          (!training_ && (row_id_ % num_folds_) == fold_id_)) {
        return underlying_reader_->Read(s);
      } else {
        T tmp;
        underlying_reader_->Read(&tmp);
        return false;
      }
    }
  }

  virtual bool ReachedEnd() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return underlying_reader_->ReachedEnd();
  }

private:
  InputRecordReader<T>* underlying_reader_;
  std::mutex mutex_;
  int fold_id_;
  int num_folds_;
  bool training_;
  int row_id_;
};

template <class T>
class CrossValidationInput : public RecordInput<T> {
public:
  CrossValidationInput(
      RecordInput<T>* underlying_input,
      int fold_id,
      int num_folds,
      bool training) : underlying_input_(underlying_input), fold_id_(fold_id), num_folds_(num_folds), training_(training) {
  }
  ~CrossValidationInput() {
    delete underlying_input_;
  }

  virtual InputRecordReader<T>* CreateReader() override {
    return new CrossValidationReader<T>(
        underlying_input_->CreateReader(), fold_id_, num_folds_, training_);
  }

private:
  RecordInput<T>* underlying_input_;
  int fold_id_;
  int num_folds_;
  bool training_;
};

#endif /* BASE_READERUTIL_H_ */
