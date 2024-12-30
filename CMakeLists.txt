cmake_minimum_required(VERSION 3.13) # CMake version check
project(app)                         # Create project "app"
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")

add_executable(app ${SOURCES})
target_compile_definitions(app PRIVATE
    RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources"
)
target_include_directories(app PRIVATE "${CMAKE_SOURCE_DIR}/glm")

if(EMSCRIPTEN)
  set_target_properties(app PROPERTIES SUFFIX ".html")
  target_link_options(app PRIVATE
	 --shell-file=${CMAKE_CURRENT_SOURCE_DIR}/custom_template.html 
	 "-sUSE_WEBGPU=1"
	 "-sUSE_GLFW=3"
	 -sALLOW_MEMORY_GROWTH
	 --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
	 )

  add_subdirectory(imgui)
  target_link_libraries(app PRIVATE imgui)
else()
  set(DAWN_FETCH_DEPENDENCIES ON)
  add_subdirectory("dawn" EXCLUDE_FROM_ALL)
  add_subdirectory(imgui)
  target_link_libraries(app PRIVATE dawn::webgpu_dawn glfw webgpu_glfw imgui)
endif()