cmake_minimum_required(VERSION 3.10..3.31)

project(cd_read)

set(CMAKE_C_STANDARD 11)

add_subdirectory(../../psyz build)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)
target_link_libraries(${PROJECT_NAME} PRIVATE psyz)
target_compile_definitions(${PROJECT_NAME} PRIVATE PSYZ_PC)
target_include_directories(psyz PRIVATE ../../psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
psyz_web_configure_target(${PROJECT_NAME})
psyz_title(${PROJECT_NAME} "Sample: CD data read")
