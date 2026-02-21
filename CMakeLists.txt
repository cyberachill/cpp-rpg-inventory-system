# ------------------------------------------------------------
# CMake minimum version – modern CMake özellikleri için 3.12+
# ------------------------------------------------------------
cmake_minimum_required(VERSION 3.12)

# ------------------------------------------------------------
# Proje tanımı
# ------------------------------------------------------------
project(RPGInventory LANGUAGES CXX)

# ------------------------------------------------------------
# C++ standart ayarı – CMake bu bayrağı otomatik ekler
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clang‑tidy / clang‑format için

# ------------------------------------------------------------
# Kaynak dosyaları (şu an sadece main.cpp, ileride eklenebilir)
# ------------------------------------------------------------
set(SRC_FILES
    src/main.cpp
    # src/other_module.cpp   <-- gelecekte ekleyebilirsiniz
)

# ------------------------------------------------------------
# Hedef (executable) oluşturma
# ------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SRC_FILES})

# ------------------------------------------------------------
# Include dizinleri – src klasöründeki .hpp'ler burada aranır
# ------------------------------------------------------------
target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ------------------------------------------------------------
# Derleyici özellikleri (C++17 zorunlu)
# ------------------------------------------------------------
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# ------------------------------------------------------------
# Uyarı / sıkı denetim bayrakları (opsiyonel ama tavsiye edilir)
# ------------------------------------------------------------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------
# Linux/macOS – pthread bağlantısı (mutex/thread güvenliği)
# ------------------------------------------------------------
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

# ------------------------------------------------------------
# Build type default (Debug) – IDE'lerde kolaylık sağlar
# ------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()
