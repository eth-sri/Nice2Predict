# Nice2Predict
Learning framework for program property prediction.

This is a backend for tools such as JSNice (http://jsnice.org/) for JavaScript that can predict program properties such as variable names and types. This backend is designed to extend the tool to multiple programming languages. For this reason, the machine learning machinery is extracted in this tool.

To get a complete tool, one must include a parses for each programming language of interest and train on a lot of code.

We have included an example frontend for JavaScript deminification at http://github.com/eth-srl/UnuglifyJS . This tool work together with the Nice2Server

## Compiling

To compile, first install dependencies (on Ubuntu 14.04):

> sudo apt-get install libgoogle-glog-dev libgflags-dev libjsoncpp-dev libmicrohttpd-dev libcurl4-openssl-dev libargtable2-dev cmake

Install Google SparseHash 2.0.2:
https://github.com/sparsehash/sparsehash

And then install json-rpc-cpp from:
https://github.com/cinemast/libjson-rpc-cpp

[Optional] Install Google Performance Tools:

1. libunwind: http://download.savannah.gnu.org/releases/libunwind/libunwind-1.1.tar.gz

2. gperftools: https://code.google.com/p/gperftools/

[Optional] Using Eclipse CDT with C++11:
http://stackoverflow.com/a/20101407

Finally, call
> ./build.sh

## Training

Compiling creates a training binary in
> bin/training/train

To get options for training, use:
> bin/training/train --help

By default, train gets input programs (converted to JSON for example with UnuglifyJS) from the file testdata in the current directory. As a result, it creates files with the trained model.

## Predicting properties

To predict properties for new programs, start a server after a model was trained:

> bin/server/nice2server --logtostderr

Then, the server will predict properties for programs given in JsonRPC format. One can debug and observe deobfuscation from the viewer available in the viewer/viewer.html .


