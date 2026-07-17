# - Try to find FFTW3
# Once done, this will define
#
#  FFTW3_FOUND - system has FFTW3
#  FFTW3_INCLUDE_DIRS - the FFTW3 include directories
#  FFTW3_LIBRARIES - link these to use FFTW3

include(LibFindMacros)

# Dependencies

set(FFTW3_PKGCONF_INCLUDE_DIRS /usr/include)
set(FFTW3_PKGCONF_LIBRARY_DIRS /usr/lib /usr/lib/x86_64-linux-gnu)
set(FFTW3_LIBRARY_NAME fftw3f)

# Include dir
find_path(FFTW3_INCLUDE_DIR
  NAMES fftw3.h
  PATHS ${FFTW3_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(FFTW3_LIBRARY
  NAMES ${FFTW3_LIBRARY_NAME}
  PATHS ${FFTW3_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(FFTW3_PROCESS_INCLUDES FFTW3_INCLUDE_DIR)
set(FFTW3_PROCESS_LIBS FFTW3_LIBRARY)
libfind_process(FFTW3)
