/*
   Copyright 2017 DeepCode GmbH

   Author: Veselin Raychev
 */

#include "single_proto.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "util/zstream/zstream.h"

void WriteCompressedProtoToFile(
    const google::protobuf::MessageLite& message,
    const std::string& filename) {
  zstr::ofstream outf(filename);
  google::protobuf::io::OstreamOutputStream zero_copy_output(&outf);
  google::protobuf::io::CodedOutputStream coded_output(&zero_copy_output);
  message.SerializeToCodedStream(&coded_output);
}
