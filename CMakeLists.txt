cmake_minimum_required(VERSION 3.16)
project(DecompM CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# O original é x86 de 32 bits. Para comparar layout de structs,
# prefira compilar em 32 bits (-m32 ou generator "Visual Studio -A Win32").

add_library(decompm_sff src/sff/sff_format.h src/sff/sff_reader.cpp)
target_include_directories(decompm_sff PUBLIC src)

foreach(t sff2png sprmake2 sndmaker)
  add_executable(${t} src/tools/${t}.cpp)
  target_link_libraries(${t} decompm_sff)
endforeach()
