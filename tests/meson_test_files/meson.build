project('hello.world', 'cpp')

compiler_args = ['-std=c++11', '-Wall', '-Wextra']

static_library('hello_lib', 'main.cpp', cpp_args: compiler_args)

executable('hello', 'main.cpp', cpp_args: compiler_args)

executable('another_executable', 'another_file.cpp', cpp_args: compiler_args)

subdir('a_subdir')
