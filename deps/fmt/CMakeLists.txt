cmake_minimum_required(VERSION 3.15)

project(fmt) 
add_library(fmt STATIC
    include/fmt/args.h
    include/fmt/chrono.h
    include/fmt/color.h
    include/fmt/compile.h
    include/fmt/core.h
    include/fmt/format.h
    include/fmt/format-inl.h
    include/fmt/locale.h
    include/fmt/os.h
    include/fmt/ostream.h
    include/fmt/posix.h
    include/fmt/printf.h
    include/fmt/ranges.h
    src/format.cc
    src/os.cc)
target_include_directories(fmt PUBLIC include)
