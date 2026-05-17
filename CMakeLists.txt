cmake_minimum_required(VERSION 3.10)
project(imx93_sorting_sim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPTONICA REQUIRED lept)
pkg_check_modules(ZBAR REQUIRED zbar)
pkg_check_modules(GPIOD REQUIRED libgpiod)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${TESSERACT_INCLUDE_DIRS}
    ${LEPTONICA_INCLUDE_DIRS}
    ${ZBAR_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

# ONNX Runtime — bundled in lib/
set(ONNXRUNTIME_LIB ${CMAKE_SOURCE_DIR}/lib/libonnxruntime.so)
if(NOT EXISTS ${ONNXRUNTIME_LIB})
    message(WARNING "libonnxruntime.so not found in lib/, TinyOCR mode will be unavailable")
endif()

# RPATH so the binary finds libonnxruntime.so at runtime
set(CMAKE_INSTALL_RPATH "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_BUILD_RPATH "${CMAKE_SOURCE_DIR}/lib")

add_executable(
    sorting_sim
    main.cpp
    src/input_source.cpp
    src/recognizer.cpp
    src/ocr_corrector.cpp
    src/rule_engine.cpp
    src/risk_engine.cpp
    src/alarm_simulator.cpp
    src/logger.cpp
    src/stats.cpp
    src/quality_engine.cpp
    src/decision_engine.cpp
    src/tiny_ocr_engine.cpp
    src/hash_logger.cpp
    src/anomaly_detector.cpp
    src/label_detector.cpp
)

target_link_libraries(
    sorting_sim
    ${OpenCV_LIBS}
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
    ${ZBAR_LIBRARIES}
    ${GPIOD_LIBRARIES}
    ${ONNXRUNTIME_LIB}
    ssl
    crypto
)
