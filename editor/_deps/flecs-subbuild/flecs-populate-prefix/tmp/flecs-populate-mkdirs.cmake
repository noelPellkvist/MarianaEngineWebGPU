# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-src")
  file(MAKE_DIRECTORY "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-src")
endif()
file(MAKE_DIRECTORY
  "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-build"
  "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix"
  "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix/tmp"
  "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix/src/flecs-populate-stamp"
  "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix/src"
  "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix/src/flecs-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix/src/flecs-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "G:/MARIANA/New/MarianaEngineWebGPU/build-web/_deps/flecs-subbuild/flecs-populate-prefix/src/flecs-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
