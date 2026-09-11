src/
├── core/                       # 100% C++ estándar (sin dependencias de SO ni terminal)
│   ├── board.hpp / .cpp        # Matriz 10x22, colisiones, SRS kicks, borrado de líneas
│   ├── tetromino.hpp / .cpp    # Definiciones de piezas, cajas y tablas SRS
│   ├── randomizer.hpp / .cpp   # 7-Bag Randomizer
│   ├─── game.hpp / .cpp        # Máquina de estados, gravedad y lógica de turnos
│   └─── constants.hpp          # Declaración de constantes  
│
├── platform/                   # Interfaces (contratos abstractos)
│   ├── audio_interface.hpp     # Interfaz virtual pura: play(), stop(), pause()
│   └── display_interface.hpp   # Interfaz virtual pura: draw_tile(), draw_text(), clear()
│
├── pc/                         # Implementación específica para PC
│   ├── mpv_audio.hpp / .cpp    # Implementa audio_interface usando mpv (pipe/subprocess)
│   ├── terminal_ui.hpp / .cpp  # Implementa display_interface (TUI/terminal)
│   └── main.cpp                # Punto de entrada para PC (bucle principal de terminal)
│
└── esp32/                      # (Futuro) Implementación para microcontrolador
    ├── i2s_audio.hpp / .cpp    # Implementa audio_interface vía DAC/I2S/Buzzer
    ├── tft_display.hpp / .cpp  # Implementa display_interface vía TFT_eSPI / LovyanGFX
    └── main_esp32.cpp          # app_main() o setup()/loop() de Arduino/ESP-IDF
