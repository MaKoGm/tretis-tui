cmake_minimum_required(VERSION 3.10)
project(tretis-tui)

# Exigimos usar C++17 (recomendado para código moderno, como std::array)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Creamos el ejecutable listando TODOS los archivos fuente (.cpp)
add_executable(tretis
    src/pc/main.cpp 
    src/pc/terminal_ui.cpp
    src/core/game.cpp
    src/core/board.cpp
    src/core/tetromino.cpp
)

# 2. Le decimos a CMake dónde está la raíz de nuestros headers
target_include_directories(tretis PRIVATE src)

# (Opcional) Activar advertencias del compilador para detectar fallos
target_compile_options(tretis PRIVATE -Wall -Wextra)

# 3. Instalación (opcional, cuando se haga 'sudo make install')
# install(TARGETS tretis DESTINATION bin)
