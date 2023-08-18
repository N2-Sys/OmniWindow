cmake_minimum_required(VERSION 3.6)

option(IS_SWITCH "Is building switch programs" OFF)
option(ENABLE_EXP_DC "Enable building the micro-benchmarks of data collection" OFF)

if(${IS_SWITCH})
  project(OmniWindow LANGUAGES)
  message(STATUS "Building switch programs.")
else()
  project(OmniWindow)
  message(STATUS "Building host programs.")
endif()

if(NOT ${IS_SWITCH})
  set(CMAKE_CXX_STANDARD 17)

  add_compile_options(-mssse3 -fcommon -march=native)

  include_directories(include)
  add_subdirectory(lib)
  add_subdirectory(tools)
endif()

function(add_p4 name source compile_flags py_script)
  if(${IS_SWITCH})
    make_directory("${CMAKE_CURRENT_BINARY_DIR}/${name}.build")

    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.sh"
      # COMMAND rm -rf *
      COMMAND cmake \"$ENV{SDE}/p4studio\"
        -DCMAKE_INSTALL_PREFIX=\"$ENV{SDE_INSTALL}\"
        -DCMAKE_MODULE_PATH=\"$ENV{SDE}/cmake\"
        -DP4_NAME=\"omniwin-${name}\"
        -DP4_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/${source}\"
        ${compile_flags}
      COMMAND make
      COMMAND make install
      COMMAND echo \"\#\! /bin/bash -i\" >"${CMAKE_CURRENT_BINARY_DIR}/${name}.sh"
      COMMAND echo \"
          if \[\[ ! -a "${CMAKE_SOURCE_DIR}/switch-shell.txt" \]\]\; then
            echo "Missing file ${CMAKE_SOURCE_DIR}/switch-shell.txt" \;
            exit\;
          fi\;
          cd \\$$SDE\;
          ./run_switchd.sh -p "omniwin-${name}" -- --background &
          ./run_bfshell.sh -f "${CMAKE_SOURCE_DIR}/switch-shell.txt" \;
          if \[\[ -a "${CMAKE_SOURCE_DIR}/switch-config.json" \]\]\; then
            cp "${CMAKE_SOURCE_DIR}/switch-config.json" /tmp/OmniWindow-config.json\;
            ./run_bfshell.sh -i -b "${CMAKE_CURRENT_SOURCE_DIR}/${py_script}" \;
          else
            echo "Missing file ${CMAKE_SOURCE_DIR}/switch-config.json" \;
            echo "Press Ctrl-C to quit..." \;
          fi\;
          fg
        \" >>"${CMAKE_CURRENT_BINARY_DIR}/${name}.sh"
      COMMAND chmod +x "${CMAKE_CURRENT_BINARY_DIR}/${name}.sh"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${name}.build"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${py_script}"
    )

    add_custom_target(${name} ALL
      DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${name}.sh"
    )
  endif()
endfunction()

add_subdirectory(apps)

if(${ENABLE_EXP_DC})
  add_subdirectory(Exp-dc)
endif()
