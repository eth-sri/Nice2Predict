#!/usr/bin/python
# This is meant to pull the thurd party code from JSON RPC here.

import sys
import os

def PrintUsage():
    sys.stderr.write("""
Usage:
    pull_json_rpc.py <directory_with_json_rpc_cpp>

""")
    exit(1)


def ExploreSourceDir(start_dir):
    prefix = start_dir + '/'
    for root, _, files in os.walk(start_dir):
        for f in files:
            if f.endswith('.h') or f.endswith('.cpp'):
                s = os.path.join(root, f)
                if s.startswith(prefix):
                    s = s[len(prefix):]
                yield s

def TransferFile(start_dir, f):
    fdir = os.path.dirname(f)
    out_filename = f.replace('/', '_')
    reader = open(os.path.join(start_dir, f), 'rt')
    writer = open(out_filename, 'wt')
    try:
        for line in reader:
            if line.startswith('#include ') and ('jsonparser.h' in line):
                line = '#include "json/json.h"\n'
            elif line.startswith('#include "'):
                include = line[10:]
                include = include[:include.find('"')]
                include = os.path.normpath(os.path.join(fdir, include))
                include = include.replace('/', '_')
                line = '#include "%s"\n' % ('json/' + include)
            elif line.startswith('#include <jsonrpccpp/'):
                include = line[21:]
                include = include[:include.find('>')]
                include = include.replace('/', '_')
                line = '#include "%s"\n' % ('json/' + include)                
            writer.write(line)
    finally:
        reader.close()
        writer.close()
    return out_filename

if __name__ == "__main__":
    if len(sys.argv) != 2:
        PrintUsage()

    start_dir = os.path.join(sys.argv[1], 'src/jsonrpccpp')

    srcs = []
    for f in ExploreSourceDir(start_dir):
        srcs.append(TransferFile(start_dir, f))

    print '#=============================================='
    print '#   Include the following into the BUILD file'
    print '#=============================================='
    print
    print 'cc_library(name = "jsonrpc",'
    print '           srcs = ['
    for x in srcs:
        print '               "%s",' % (x)
    print '           ],'
    print '           deps = [":jsoncpp"],'
    print '           linkopts = ["-lcurl", "-lmicrohttpd"],'
    print '           visibility = ["//visibility:public"])'


