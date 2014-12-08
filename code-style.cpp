// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

/// Program source for code style example.

// Use absolute path from root include directory. This would normally be
//  #include <mu/n/c.h>
#include <code-style.h>

// Prefer using individual names rather than entire namspaces.
using mu::n::c;

/// The program's main function.
int main(const int argc, const char** const argv)
{

    typedef c<int> counter_value_t;
    counter_value_t initial_value;
    counter_value_t next_value = initial_value.get_next();
    return 0;
}
