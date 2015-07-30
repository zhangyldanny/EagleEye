CMAKE_POLICY(SET CMP0020 NEW)
IF(${CMAKE_VERSION} VERSION_GREATER 3.1)
CMAKE_POLICY(SET CMP0053 OLD)
ENDIF()
set(Boost_USE_STATIC_LIBS @Boost_USE_STATIC_LIBS@)
set(Boost_USE_STATIC @Boost_USE_STATIC@)
set(Boost_USE_MULTITHREAD @Boost_USE_MULTITHREAD@)
set(BOOST_INCLUDEDIR "@Boost_INCLUDE_DIR@")
set(Boost_NO_BOOST_CMAKE ON)
set(BOOST_REQUIRED_MODULES system filesystem thread date_time iostreams chrono)
LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Boost 1.47.0 REQUIRED COMPONENTS ${BOOST_REQUIRED_MODULES})
find_package(CUDA REQUIRED)
FIND_PACKAGE(libusb-1.0 REQUIRED)
find_package(OpenCV 3.0 REQUIRED core imgproc highgui cudaimgproc cudawarping cudafeatures2d cudaoptflow cudacodec cudastereo)
IF(libusb-1.0_FOUND)
ADD_DEFINITIONS(${DEFS})
LINK_DIRECTORIES(${LINK_DIRS})
INCLUDE_DIRECTORIES(
    ${EagleLib_INCLUDE_DIRECTORIES}
    ${OpenCV_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${CUDA_INCLUDE_DIRS}
    ${LIBUSB_1_INCLUDE_DIR}
)

file(GLOB_RECURSE src "*.cpp")
file(GLOB_RECURSE hdr "*.h" "*.hpp" "*.cuh")

IF(WIN32)

ELSE()
    SET(CUDA_NVCC_FLAGS "-ccbin g++;-O2;-std=c++11;${CUDA_NVCC_FLAGS}")
ENDIF()

cuda_add_library(freenect SHARED ${src} ${hdr} ${knl})
MESSAGE(STATUS
"=========== Dev ============
    Defs: ${DEFS}
")
add_dependencies(freenect EagleLib)
target_link_libraries(freenect
    EagleLib
    libfreenect
    libfreenect_sync
    ${OpenCV_LIBS}
    ${CUDA_CUBLAS_LIBRARIES}
    ${LINK_LIBS}
    ${LIBUSB_1_LIBRARY}
)
ENDIF(libusb-1.0_FOUND)