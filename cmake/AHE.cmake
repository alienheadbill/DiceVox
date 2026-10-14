# ---------------------------------------------------------------------------
# Alien Head Entertainment (AHE) — company identity shared by every plugin.
#
# DO NOT CHANGE these values once plugins have shipped. Hosts (FL Studio,
# Logic, ...) identify a plugin by manufacturer code + plugin code; changing
# either makes saved projects lose the plugin.
#
# This file is meant to move into a shared "ahe-core" repo once there is a
# second plugin. Each plugin then only picks its own 4-character PLUGIN_CODE
# (registered in the AHE plugin ID list in the project docs).
# ---------------------------------------------------------------------------

set(AHE_COMPANY_NAME        "AHE")                       # shown as vendor in hosts
set(AHE_COMPANY_FULL_NAME   "Alien Head Entertainment")
set(AHE_COMPANY_COPYRIGHT   "Alien Head Entertainment")
set(AHE_MANUFACTURER_CODE   Ahen)                         # 4 chars: 1st upper, rest lower (Apple/GarageBand rule)
set(AHE_BUNDLE_ID_PREFIX    "com.alienheadentertainment")  # Mac bundle IDs: <prefix>.<plugin>

# Creates an AHE plugin target with the shared company identity filled in.
# Usage:
#   ahe_add_plugin(DiceVox
#       PRODUCT_NAME "DiceVox"
#       PLUGIN_CODE  Dvox
#       <any other juce_add_plugin options>)
function(ahe_add_plugin target)
    cmake_parse_arguments(AHE "" "PRODUCT_NAME;PLUGIN_CODE" "" ${ARGN})

    if(NOT AHE_PRODUCT_NAME OR NOT AHE_PLUGIN_CODE)
        message(FATAL_ERROR "ahe_add_plugin(${target}): PRODUCT_NAME and PLUGIN_CODE are required")
    endif()

    string(LENGTH "${AHE_PLUGIN_CODE}" _len)
    if(NOT _len EQUAL 4 OR NOT AHE_PLUGIN_CODE MATCHES "^[A-Z][a-z0-9]+$")
        message(FATAL_ERROR "ahe_add_plugin(${target}): PLUGIN_CODE '${AHE_PLUGIN_CODE}' must be 4 characters, "
                            "first uppercase, rest lowercase/digits (e.g. Dvox)")
    endif()

    string(TOLOWER "${target}" _lower)

    juce_add_plugin(${target}
        COMPANY_NAME             "${AHE_COMPANY_NAME}"
        COMPANY_COPYRIGHT        "${AHE_COMPANY_COPYRIGHT}"
        PLUGIN_MANUFACTURER_CODE ${AHE_MANUFACTURER_CODE}
        PLUGIN_CODE              ${AHE_PLUGIN_CODE}
        PRODUCT_NAME             "${AHE_PRODUCT_NAME}"
        BUNDLE_ID                "${AHE_BUNDLE_ID_PREFIX}.${_lower}"
        ${AHE_UNPARSED_ARGUMENTS})
endfunction()
