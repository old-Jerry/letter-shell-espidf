cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(HTX-ESP32-program)
#使用相对路径指定组件目录-但是在根目录下的无需声明，编译器会自动查找
set(EXTRA_COMPONENT_DIRS
    components/lettershell
)