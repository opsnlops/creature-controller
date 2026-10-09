
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Enable shlibs for this project
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

set(CPACK_DEBIAN_PACKAGE_NAME "creature-controller")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "April's Creature Workshop Controller")

set(CPACK_PACKAGE_DESCRIPTION
        "Allows for control of a creature from April's Creature
         Workshop. Communicates with the creature over serial,
         both UART and USB-CDC are supported.")

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

set(CPACK_DEBIAN_PACKAGE_DEPENDS "locales-all, zlib1g, libsasl2-2, libicu76, libasound2t64, libutf8proc3, libuuid1, libcurl4, libprotobuf32t64")

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
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_NAME "creature-audio-router")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_DEPENDS "libasound2t64, init-system-helpers")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_DESCRIPTION "Multichannel Opus/RTP audio routing for Creature Workshop HDMI speakers")
set(CPACK_DEBIAN_CREATURE-AUDIO-ROUTER_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/audio-router/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
include(CPack)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackAudioRouterConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/CPackAudioRouterConfig.cmake" @ONLY)
