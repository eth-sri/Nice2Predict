# Nice2Predict
Learning framework for program property prediction.

This is a backend for tools such as JSNice (http://jsnice.org/) for JavaScript that can predict program properties such as variable names and types. This backend is designed to extend the tool to multiple programming languages. For this reason, the machine learning machinery is extracted in this tool.

To get a complete tool, one must include a parses for each programming language of interest and train on a lot of code.

We have included an example frontend for JavaScript deminification at http://github.com/eth-srl/UnuglifyJS . This tool work together with the Nice2Server

## Compiling

To compile, first install dependencies

on Ubuntu:
> sudo apt-get install libmicrohttpd-dev libcurl4-openssl-dev bazel libgoogle-glog-dev libgflags-dev

on Mac:
> brew tap caskroom/versions
> brew cask install java8
> brew install libmicrohttpd bazel glog gflags

on Windows follow any installation instructions and install libmicrohttpd, curl and bazel.

[Optional] Install Google Performance Tools:

1. libunwind: http://download.savannah.gnu.org/releases/libunwind/libunwind-1.1.tar.gz

2. gperftools: https://code.google.com/p/gperftools/

Finally, call
> bazel build //...

If using a sufficiently new compiler, boringssl dependency for gRPC may fail to build. Try:
> bazel build --copt=-Wno-error=array-parameter --copt=-Wno-error=stringop-overflow //...

To run tests, call
> bazel test //...

## Training

Run:
> bazel run //n2p/training:train
Don't forget about `--copt` from above if boringssl fails to build.

To get options for training, use:
> bazel run //n2p/training:train --help

By default, train gets input programs (converted to JSON for example with UnuglifyJS) from the file testdata in the current directory. As a result, it creates files with the trained model.

If you wish to train the model using pseudolikelihood use the following parameters:

> bazel run //n2p/training:train -- -training_method pl -input path/to/input/file --logtostderr

you can control the pseudolikelihood specific beam size with the `-beam_size` parameter which is different from the beam size used during MAP Inference.

`//n2p/training:train` expects data to be in protobuf recordIO format. If you want to use JSON input - use `//n2p/training:train_json` instead.

### Factors

by default the usage of factor features in Nice2Predict is enabled, however if you wish to disable it you can launch the training with the following command:

> bazel run //n2p/training:train -- -use_factors=false -input path/to/input/file --logtostderr

## Predicting properties

To predict properties for new programs, start a server after a model was trained:

> bazel run //src/server/nice2serverproto -- --logtostderr

To run old JsonRPC API:
> bazel run //src/server/nice2server -- --logtostderr

One can debug and observe deobfuscation from the viewer available in the viewer/viewer.html .
