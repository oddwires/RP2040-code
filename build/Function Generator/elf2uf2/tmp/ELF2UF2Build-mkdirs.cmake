# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Program Files (x86)/Pico/pico-sdk/tools/elf2uf2"
  "C:/Program Files (x86)/Pico/RPI - pico/build/elf2uf2"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/elf2uf2"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/elf2uf2/tmp"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/elf2uf2/src/ELF2UF2Build-stamp"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/elf2uf2/src"
  "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/elf2uf2/src/ELF2UF2Build-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Program Files (x86)/Pico/RPI - pico/build/Function Generator/elf2uf2/src/ELF2UF2Build-stamp/${subDir}")
endforeach()
