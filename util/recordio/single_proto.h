/*
   Copyright 2017 DeepCode GmbH

   Author: Veselin Raychev
 */

#ifndef UTIL_RECORDIO_SINGLE_PROTO_H_
#define UTIL_RECORDIO_SINGLE_PROTO_H_

#include <string>
#include <google/protobuf/message_lite.h>

void WriteCompressedProtoToFile(
    const google::protobuf::MessageLite& message,
    const std::string& filename);


#endif /* UTIL_RECORDIO_SINGLE_PROTO_H_ */
