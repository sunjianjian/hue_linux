cmake_minimum_required(VERSION 3.6)
project(smartlink_controller)

# 清替换成您的交叉编译工具
#set(CMAKE_C_COMPILER "/opt/armeabi-api19/bin/arm-linux-androideabi-gcc")
#set(CMAKE_CXX_COMPILER "/opt/armeabi-api19/bin/arm-linux-androideabi-g++")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --sysroot=/opt/armeabi-api19/sysroot")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 --sysroot=/opt/armeabi-api19/sysroot")

# 生成的可执行文件存放路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

# 头文件路径
include_directories(.)

# 库文件路径
link_directories(".")

# 源文件
set(DEMO_SOURCE_FILES
        demo.cpp
        hue.cpp
        )

# 生成可执行文件
add_executable(demo ${DEMO_SOURCE_FILES})

# 依赖的第三方库
target_link_libraries(demo pthread curl)
