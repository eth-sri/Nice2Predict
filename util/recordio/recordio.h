/*
   Copyright 2017 DeepCode GmbH

   Author: Veselin Raychev
 */

#ifndef UTIL_RECORDIO_RECORDIO_H_
#define UTIL_RECORDIO_RECORDIO_H_

#include <fstream>
#include <memory>

#include "google/protobuf/message_lite.h"

namespace google { namespace protobuf { namespace io {
class OstreamOutputStream;
class CodedOutputStream;
class IstreamInputStream;
}}}

class RecordWriter {
public:
  RecordWriter(const std::string& filename);
  RecordWriter(const std::string& filename, bool append);
  ~RecordWriter();

  void Write(const google::protobuf::MessageLite& message);

  void Flush();
  void Close();
private:
  std::ofstream out_file_;
  std::unique_ptr<google::protobuf::io::OstreamOutputStream> out_stream_;
  std::unique_ptr<google::protobuf::io::CodedOutputStream> coded_stream_;
};


class RecordReader {
public:
  RecordReader(const std::string& filename);
  ~RecordReader();

  bool Read(google::protobuf::MessageLite* message);

  // Read, but allows parsing to fail.
  bool ReadMayNotParse(google::protobuf::MessageLite* message, bool* parsed);

  void Close();

private:
  std::ifstream in_file_;
  std::unique_ptr<google::protobuf::io::IstreamInputStream> in_stream_;
};

template<class ProtoClass>
void ReadRecordsIntoVector(const std::string& filename, std::vector<ProtoClass>* v) {
  RecordReader reader(filename);
  ProtoClass tmp;
  while (reader.Read(&tmp)) {
    v->emplace_back();
    v->back().Swap(&tmp);
  }
}

template<class ProtoClass>
void WriteRecordsToFile(const std::string& filename, const std::vector<ProtoClass>& v) {
  RecordWriter writer(filename);
  for (size_t i = 0; i < v.size(); ++i) {
    writer.Write(v[i]);
  }
}

// Compressed Reader/Writer

class RecordCompressedWriter {
public:
  RecordCompressedWriter(const std::string& filename);
  ~RecordCompressedWriter();

  void Write(const google::protobuf::MessageLite& message);

  void Close();

private:
  std::unique_ptr<std::ostream> out_file_;
  std::unique_ptr<google::protobuf::io::OstreamOutputStream> out_stream_;
  std::unique_ptr<google::protobuf::io::CodedOutputStream> coded_stream_;
};

class RecordCompressedReader {
public:
  RecordCompressedReader(const std::string& filename);
  ~RecordCompressedReader();

  bool Read(google::protobuf::MessageLite* message);

  void Close();

private:
  std::unique_ptr<std::istream> in_file_;
  std::unique_ptr<google::protobuf::io::IstreamInputStream> in_stream_;
};


#endif /* UTIL_RECORDIO_RECORDIO_H_ */
