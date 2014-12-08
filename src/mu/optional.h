// vim: set ts=4 sw=4 tw=80 expandtab
// Copyright 2014 Migrant Coder

#pragma once

#include <experimental/optional>

namespace mu {  
    /// Convenience alias.
    template <typename T> using optional = std::experimental::optional<T>;
} // namespace mu
