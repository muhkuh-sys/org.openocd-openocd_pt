cmake_minimum_required(VERSION 3.7)

#----------------------------------------------------------------------------
#
# Build libusb and create a CMake component.
#
PROJECT("libusb")

SET(VERSION_EXTERNAL "1.0.24")
SET(VERSION_EXTERNAL_POST "")

INCLUDE(ExternalProject)

#----------------------------------------------------------------------------
#
# Build the project.
#
ExternalProject_Add(TARGET_libusb
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}.tar.bz2
                    URL_HASH SHA1=d8d614b538f7c953b6e3b73f1eea5dc70820a7e2
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CFLAGS=${CMAKE_C_FLAGS}" "CPPFLAGS=${CMAKE_CXX_FLAGS}" ${CMAKE_CURRENT_BINARY_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}/src/TARGET_libusb/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/install --enable-static=yes --enable-shared=no --with-pic ${CONFIGURE_HOST}
                    BUILD_COMMAND make
                    INSTALL_COMMAND make install
)
