cmake_minimum_required(VERSION 3.22) # CMake version check
project(app)                         # Create project "app"
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

file(GLOB_RECURSE APP_SOURCES
     CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_executable(app ${APP_SOURCES})

target_include_directories(app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(external/glm)
target_link_libraries(app PRIVATE glm::glm)

set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
add_subdirectory("dawn" EXCLUDE_FROM_ALL)

if(EMSCRIPTEN)
  set_target_properties(app PROPERTIES SUFFIX ".html")
  target_link_libraries(app PRIVATE emdawnwebgpu_cpp webgpu_glfw)
  target_link_options(app PRIVATE "-sASYNCIFY=1" "-sUSE_GLFW=3")
else()
  target_link_libraries(app PRIVATE webgpu_dawn webgpu_glfw glfw)
endif()
