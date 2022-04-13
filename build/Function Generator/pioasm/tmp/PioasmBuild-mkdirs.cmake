# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Program Files (x86)/Pico/pico-sdk/tools/pioasm"
  "C:/Program Files (x86)/Pico/RPI - pico/build/pioasm"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/pioasm"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/pioasm/tmp"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/pioasm/src/PioasmBuild-stamp"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/pioasm/src"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/pioasm/src/PioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/pioasm/src/PioasmBuild-stamp/${subDir}")
endforeach()
