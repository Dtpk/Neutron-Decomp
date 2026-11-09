# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-src")
  file(MAKE_DIRECTORY "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-src")
endif()
file(MAKE_DIRECTORY
  "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-build"
  "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix"
  "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix/tmp"
  "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix/src/sdl2_mixer-populate-stamp"
  "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix/src"
  "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix/src/sdl2_mixer-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix/src/sdl2_mixer-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspace/fix-split/Neutron-Port-Android/neutron-port-universal/build-android-armv7/_deps/sdl2_mixer-subbuild/sdl2_mixer-populate-prefix/src/sdl2_mixer-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
