CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

SET(VERSION_EXTERNAL "0.11.0")
SET(VERSION_PACKAGE "1")

PROJECT("openocd")

SET(PROJECT_VERSION "${VERSION_EXTERNAL}.${VERSION_PACKAGE}")

INCLUDE(ExternalProject)


# Python is required for the patch script.
FIND_PACKAGE(PythonInterp 2.7 REQUIRED)


ExternalProject_Add(TARGET_openocd
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openocd
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/openocd-0.11.0-rc2.tar.bz2
                    URL_HASH SHA1=ec8642a5df994628975c8107306ba6bc7a12039d
                    PATCH_COMMAND "${PYTHON_EXECUTABLE}" ${CMAKE_HOME_DIRECTORY}/../cmake/tools/apply_patches.py --working-folder ${CMAKE_CURRENT_BINARY_DIR}/openocd/src/TARGET_openocd --patch-folder ${CMAKE_CURRENT_SOURCE_DIR}/patches --strip 1 --copy-folder ${CMAKE_CURRENT_SOURCE_DIR}/copy
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "PKG_CONFIG_PATH=${CMAKE_CURRENT_BINARY_DIR}/libusb/install/lib/pkgconfig" ${CMAKE_CURRENT_BINARY_DIR}/openocd/src/TARGET_openocd/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/install --enable-static=yes --enable-shared=no --with-pic ${CONFIGURE_HOST}
                    BUILD_COMMAND make
                    INSTALL_COMMAND make install
)
