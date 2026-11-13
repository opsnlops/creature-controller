
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Enable shlibs for this project
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

set(CPACK_DEBIAN_PACKAGE_NAME "creature-controller")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
# The router's summary is set in CPackAudioRouterConfig.cmake.in
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Animatronic creature controller for April's Creature Workshop")

set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# Pull the version from the project
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# Use a timestamp for the release number
string(TIMESTAMP NOW "%s")
set(CPACK_DEBIAN_PACKAGE_RELEASE ${NOW})

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "April White <april@opsnlops.io>")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# shlibdeps adds the linked libraries. locales-all provides the en_US.UTF-8 locale
# that SpdlogLogger sets at startup, which shlibdeps can't see.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "locales-all, libasound2t64, libcurl4, udev, avahi-daemon, systemd")

# Build independently installable products from the same shared implementation.
set(CPACK_DEB_COMPONENT_INSTALL ON)
if(CREATURE_BUILD_CONTROLLER)
    set(CPACK_COMPONENTS_ALL creature-controller)
else()
    set(CPACK_COMPONENTS_ALL creature-audio-router)
    set(CPACK_PACKAGE_VERSION "${CREATURE_AUDIO_ROUTER_VERSION}")
    unset(CPACK_DEBIAN_PACKAGE_RELEASE)
endif()
set(CPACK_DEBIAN_CREATURE-CONTROLLER_PACKAGE_NAME "creature-controller")
# Extended descriptions; the summary line comes from CPACK_PACKAGE_DESCRIPTION_SUMMARY.
set(CPACK_DEBIAN_CREATURE-CONTROLLER_DESCRIPTION
    "Receives DMX frames for one creature over E1.31 or RS-485, turns them into
motor positions, and streams them over USB serial to the creature's
Raspberry Pi Pico board. It also plays the creature's voice and background
music from the Creature Server's Opus/RTP streams in time with its motion.
A udev rule starts one instance for each plugged-in board, and every
creature's configuration ships in the package.")
set(CPACK_DEBIAN_CREATURE-CONTROLLER_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/controller/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/packaging/controller/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/controller/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/controller/postrm")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_NAME "creature-audio-router")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_DEPENDS "libasound2t64, avahi-daemon, systemd")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_DESCRIPTION
    "Receives the Creature Server's Opus/RTP multicast streams for several
creatures and the background music, mixes them according to each speaker's
routes, and plays the result through a multichannel ALSA device such as an
HDMI receiver. It stays in time with the creatures using the server's RTCP
timing and PTP-synchronized clocks.")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
include(CPack)

# Debian requires a changelog in each package's doc directory (Policy 12.7). The
# history itself lives in git, so each build gets a single entry for its version.
# The controller carries a Debian revision, so its changelog is changelog.Debian.gz;
# the router has none, which makes it a native package with changelog.gz.
string(TIMESTAMP CREATURE_CHANGELOG_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)
function(creature_debian_changelog package version file component)
    set(changelog "${CMAKE_CURRENT_BINARY_DIR}/changelog/${package}/${file}")
    file(WRITE "${changelog}"
         "${package} (${version}) unstable; urgency=medium\n\n"
         "  * Automated build. The change history is in the project's git log:\n"
         "    https://github.com/opsnlops/creature-controller\n\n"
         " -- ${CPACK_DEBIAN_PACKAGE_MAINTAINER}  ${CREATURE_CHANGELOG_DATE}\n")
    execute_process(COMMAND "${CREATURE_MAN_GZIP}" -9 -n -f "${changelog}" COMMAND_ERROR_IS_FATAL ANY)
    install(FILES "${changelog}.gz" DESTINATION share/doc/${package} COMPONENT ${component})
endfunction()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CREATURE_BUILD_CONTROLLER)
        creature_debian_changelog(creature-controller "${PROJECT_VERSION}-${NOW}" changelog.Debian
                                  creature-controller)
    endif()
    creature_debian_changelog(creature-audio-router "${CREATURE_AUDIO_ROUTER_VERSION}" changelog
                              creature-audio-router)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackAudioRouterConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/CPackAudioRouterConfig.cmake" @ONLY)
