cmake_minimum_required(VERSION 3.16)
project(DecompM CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# O original é x86 de 32 bits. Para comparar layout de structs,
# prefira compilar em 32 bits (-m32 ou generator "Visual Studio -A Win32").

add_library(decompm_sff src/sff/sff_format.h src/sff/sff_reader.cpp src/sff/sff_decode.cpp
            src/sff/png_writer.cpp src/sff/sff_extract.cpp)
target_include_directories(decompm_sff PUBLIC src)

add_library(decompm_parser src/parser/ini_parser.h src/parser/ini_parser.cpp
            src/parser/air_parser.h src/parser/air_parser.cpp
            src/parser/cns_parser.h src/parser/cns_parser.cpp src/parser/cns_controllers.h
            src/parser/expr.h src/parser/expr.cpp)
target_include_directories(decompm_parser PUBLIC src)

foreach(t defdump airdump cnsdump exprcheck)
  add_executable(${t} src/tools/${t}.cpp)
  target_link_libraries(${t} decompm_parser)
endforeach()

foreach(t sff2png sprmake2 sndmaker)
  add_executable(${t} src/tools/${t}.cpp)
  target_link_libraries(${t} decompm_sff)
endforeach()

add_library(decompm_engine src/engine/character.h src/engine/character.cpp src/engine/player.h src/engine/player.cpp)
target_link_libraries(decompm_engine decompm_parser)
target_include_directories(decompm_engine PUBLIC src)

enable_testing()
add_executable(expr_spec_tests tests/expr_spec_tests.cpp)
target_link_libraries(expr_spec_tests decompm_parser)
add_test(NAME expr_spec_tests COMMAND expr_spec_tests)

# Teste de integracao com o KFM real (precisa da sua copia do MUGEN): cmake -DMUGEN_DIR=/caminho/mugen-1.1b1
if(MUGEN_DIR)
  add_executable(engine_kfm_test tests/engine_kfm_test.cpp)
  target_link_libraries(engine_kfm_test decompm_engine)
  add_test(NAME engine_kfm_test COMMAND engine_kfm_test ${MUGEN_DIR})
endif()
