# 指定目标系统是Windows
set(CMAKE_SYSTEM_NAME Windows)

# 指定C和C++编译器（这里以64位为例）
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# 指定库和头文件的查找路径
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# 设置查找模式，避免找到Linux本地的库
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
 