
GET_PROPERTY(INCLUDE_DIRS TARGET ${PROJECT_NAME} PROPERTY INCLUDE_DIRECTORIES)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Plugins)
set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Plugins)
ocv_add_precompiled_header_to_target(${PROJECT_NAME} src/precompiled.hpp)
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_config.txt")
  FILE(READ "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_config.txt" temp)
endif()
SET(PROJECT_ID)
IF(temp)
  STRING(FIND "${temp}" "\n" len)
  STRING(SUBSTRING "${temp}" 0 ${len} temp)
  SET(PROJECT_ID ${temp})
ELSE(temp)
  SET(PROJECT_ID "1")
ENDIF(temp)
LIST(REMOVE_DUPLICATES INCLUDE_DIRS)
LIST(REMOVE_DUPLICATES LINK_DIRS_RELEASE)
LIST(REMOVE_DUPLICATES LINK_DIRS_DEBUG)
if(WIN32)
	string(REGEX REPLACE "-D" "/D" WIN_DEFS "${DEFS}")
	string(REGEX REPLACE ";" " " WIN_DEFS "${WIN_DEFS}")
	FILE(WRITE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Debug/${PROJECT_NAME}_config.txt" "${PROJECT_ID}\n${INCLUDE_DIRS};\n${LINK_DIRS_DEBUG};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Debug\n${LINK_DIRS_RELEASE}${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/RelWithDebInfo;\n/DPROJECT_BUILD_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" ${WIN_DEFS} /DPLUGIN_NAME=${PROJECT_NAME} /FI\"EagleLib/Detail/PluginExport.hpp\"")
	FILE(WRITE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/RelWithDebInfo/${PROJECT_NAME}_config.txt" "${PROJECT_ID}\n${INCLUDE_DIRS};\n${LINK_DIRS_DEBUG};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Debug\n${LINK_DIRS_RELEASE}${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/RelWithDebInfo;\n/DPROJECT_BUILD_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" ${WIN_DEFS} /DPLUGIN_NAME=${PROJECT_NAME} /FI\"EagleLib/Detail/PluginExport.hpp\"")
else(WIN32)
	FILE(WRITE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${PROJECT_NAME}_config.txt" "${PROJECT_ID}\n${INCLUDE_DIRS};\n${LINK_DIRS_DEBUG};${LINK_DIRS}\n${LINK_DIRS_RELEASE};${LINK_DIRS}\n-DPROJECT_BUILD_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" ${DEFS} -DPLUGIN_NAME=${PROJECT_NAME} -include \"EagleLib/Detail/PluginExport.hpp\"")
endif(WIN32)

ADD_DEFINITIONS(-DPROJECT_CONFIG_FILE=\"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_config.txt\")
ADD_DEFINITIONS(-DPLUGIN_NAME=${PROJECT_NAME})


if(WIN32)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FI\"EagleLib/Detail/PluginExport.hpp\"")
else(WIN32)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include \"EagleLib/Detail/PluginExport.hpp\"")
endif(WIN32)

LINK_DIRECTORIES(${LINK_DIRS_DEBUG})
LINK_DIRECTORIES(${LINK_DIRS_RELEASE})
LINK_DIRECTORIES(${LINK_DIRS})
INSTALL(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
       
IF(RCC_VERBOSE_CONFIG) 
  string(REGEX REPLACE ";" "\n    " include_dirs_ "${INCLUDE_DIRS}")
  string(REGEX REPLACE ";" "\n    " link_dirs_release_ "${LINK_DIRS_RELEASE}")
  string(REGEX REPLACE ";" "\n    " link_dirs_debug_ "${LINK_DIRS_DEBUG}")
  MESSAGE(STATUS
  "  Include Dirs:
    ${include_dirs_}  
  Link Dirs Debug: 
    ${link_dirs_debug_}
  Link Dirs Release: 
    ${link_dirs_release_}
 ")
ENDIF(RCC_VERBOSE_CONFIG)
