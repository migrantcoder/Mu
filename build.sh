#!/bin/bash

# Run from project root.

# Pass args to compiler.
CC_ARGS=${@:2}

BUILDDIR=build
SRCDIR=src
TSTDIR=tst
PERFDIR=perf

function die() {
    local error_code=$?;
    echo "Build step failed, exiting ..." >&2;
    exit $error_code;
}

function mkbuilddir() {
    if [[ ! -d $BUILDDIR ]]
    then
        mkdir $BUILDDIR;
    fi
}


# Convert file path to file name.
#
# E.g. "perf/mu/lf/stack.cpp" -> "perf-mu-lf-stack.cpp"
function path_to_filename() {
    echo $1 | sed 's/\//\-/g';
}

# Remove filename extensions.
#
# E.g. "foo.bar.baz" -> "foo"
function remove_extensions() {
    local filename=$1;
    local truncated=$(echo $filename | cut -f 1 -d '.');
    echo -n $truncated;
}

function build_docs() {
    doxygen Doxyfile;
}

# Build programs from source files in the specified directory tree.
#
# Note that as Mu is a header only library, each .cpp source file is expected to
# define a main() function for its respective program, test or performance
# benchmark executable.
function build_executables() {
    local dir=$1;
    echo "Building programs in ${dir} ...";
    local source_names=$(find $dir -name '*.cpp');
    for source_name in $source_names
    do
        local filename=$(path_to_filename $source_name);
        local executable_name=$(remove_extensions $filename);
        echo "Building executable ${executable_name} ...";
        clang++                                                                \
            --std=c++1y                                                        \
            -stdlib=libc++                                                     \
            -pthread                                                           \
            -ggdb3                                                             \
            -O3                                                                \
            -I${dir}                                                           \
            -I${SRCDIR}                                                        \
            -Weverything                                                       \
            -Wc++11-compat                                                     \
            -Wno-c++98-compat                                                  \
            -Wno-documentation                                                 \
            -Wno-global-constructors                                           \
            -Wno-exit-time-destructors                                         \
            -Wno-missing-prototypes                                            \
            -o ${BUILDDIR}/${executable_name}                                  \
            $source_name                                                       \
            $CC_ARGS                                                           \
        ;
    done
}

mkbuilddir || die;
build_executables "tst" || die;
build_executables "perf" || die;
build_docs || echo "Can't find doxygen executable, skipping doc generation">&2;

exit 0;
