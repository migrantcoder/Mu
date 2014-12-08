#!/bin/bash

# Run from project root.

BUILDDIR=build

TEST_EXECUTABLES=$(find $BUILDDIR -d 1 -name 'tst-*' -not -name '*.*');

EC=0;
for test in $TEST_EXECUTABLES
do
    echo -n "running ${test} ... ";
    if ! $test
    then
        echo "FAIL";
        EC=1;
    else
        echo "PASS";
    fi
done

exit $EC;
