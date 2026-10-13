# The Emscripten setup every app's web build shares: its HTML shell, memory and stack, GLFW,
# ASYNCIFY (the engine waits on WebGPU synchronously) and the preloaded resource folders.
#
#   mariana_web_app(<target> OUTPUT_NAME <name> SHELL <html file> PRELOAD <dir>...)
function(mariana_web_app target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OUTPUT_NAME;SHELL" "PRELOAD")

  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${ARG_OUTPUT_NAME}"
    SUFFIX ".html"
    LINK_DEPENDS "${ARG_SHELL}"
  )

  target_link_options(${target} PRIVATE
    "SHELL:--shell-file ${ARG_SHELL}"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sINITIAL_MEMORY=256MB"
    "SHELL:-sSTACK_SIZE=262144"
    "-sASYNCIFY=1"
    "-sUSE_GLFW=3"
    "SHELL:-sFORCE_FILESYSTEM=1"
  )
  foreach(dir IN LISTS ARG_PRELOAD)
    target_link_options(${target} PRIVATE "SHELL:--preload-file ${dir}")
  endforeach()
endfunction()
