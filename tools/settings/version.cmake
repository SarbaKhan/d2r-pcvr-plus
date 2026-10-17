# The mod's version for d2r_vr_settings.exe: the plugin's own D2RPCVR_VERSION in
# vr/pcvr_identity.h, the one number (vrcam.cpp's PluginInfo and vrcam.rc's
# VERSIONINFO use it too). Writes OUT only when the number changed, so editing
# the header does not rebuild the settings program; touches STAMP if given.
#   cmake -DSRC=vr/pcvr_identity.h -DOUT=<header> [-DSTAMP=<file>] -P version.cmake
file(STRINGS "${SRC}" _line REGEX "#define D2RPCVR_VERSION \"[0-9.]+\"" LIMIT_COUNT 1 ENCODING UTF-8)
if(NOT _line MATCHES "#define D2RPCVR_VERSION \"([0-9.]+)\"")
    message(FATAL_ERROR "No #define D2RPCVR_VERSION \"x.y.z\" in ${SRC}: the settings program would not know its version")
endif()
string(CONCAT _text
    "// Made from vr/pcvr_identity.h (D2RPCVR_VERSION) by tools/settings/version.cmake - do not edit.\n"
    "#pragma once\n"
    "#define D2RVR_VERSION \"${CMAKE_MATCH_1}\"\n"
    "#define D2RVR_VERSION_W L\"${CMAKE_MATCH_1}\"\n")
set(_old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _old)
endif()
if(NOT _old STREQUAL _text)
    file(WRITE "${OUT}" "${_text}")
endif()
if(STAMP)
    file(TOUCH "${STAMP}")
endif()
