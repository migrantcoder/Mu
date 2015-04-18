#!/bin/bash

# Run from project root.

BUILDDIR=build

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

function build_docs() {
    doxygen Doxyfile;
}

mkbuilddir || die;
pushd $BUILDDIR 2>&1
cmake .. || die;
make || die;
popd 2>&1
build_docs || echo "Can't find doxygen executable, skipping doc generation">&2;

exit 0;
