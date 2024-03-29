CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

SET(VERSION_EXTERNAL "0.12.0")

PROJECT("openocd")

SET(PROJECT_VERSION "${VERSION_EXTERNAL}")

INCLUDE(ExternalProject)


ExternalProject_Add(TARGET_openocd
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openocd
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/openocd-0.12.0.tar.bz2
                    URL_HASH SHA1=a5ab82ba1d4b13634fbb4556699714d5996e37bb
                    PATCH_COMMAND "${PYTHON_INTERPRETER}" ${CMAKE_HOME_DIRECTORY}/cmake/tools/apply_patches.py --working-folder ${CMAKE_CURRENT_BINARY_DIR}/openocd/src/TARGET_openocd --patch-folder ${CMAKE_CURRENT_SOURCE_DIR}/patches --strip 1 --copy-folder ${CMAKE_CURRENT_SOURCE_DIR}/copy
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CFLAGS=-fPIC" "PKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/external/libusb/install/lib/pkgconfig" ${CMAKE_CURRENT_BINARY_DIR}/openocd/src/TARGET_openocd/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/install --enable-static=yes --enable-shared=no --with-pic ${CONFIGURE_HOST} --enable-ftdi --enable-stlink --enable-ti-icdi --enable-ulink --enable-usb-blaster-2 --enable-ft232r --enable-vsllink --enable-xds110 --enable-cmsis-dap-v2 --enable-osbdm --enable-opendous --enable-aice --enable-jlink
                    BUILD_COMMAND make
                    INSTALL_COMMAND make install
)
ADD_DEPENDENCIES(TARGET_openocd TARGET_libusb)
