include(FetchContent)

cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

# ebp_checksum_dir 计算指定文件夹的md5值
function(ebp_checksum_dir SOURCE_DIR DIGEST)
  if(UNIX AND NOT APPLE)
    execute_process(
      COMMAND find ${SOURCE_DIR} -type f -exec md5sum {} \;
      COMMAND awk "{ print $1 }"
      COMMAND sort
      COMMAND md5sum
      COMMAND awk "{ print $1 }"
      OUTPUT_VARIABLE RESULT)
  elseif(APPLE)
    execute_process(
      COMMAND find ${SOURCE_DIR} -type f -exec md5 {} \;
      COMMAND awk "{ print $NF }"
      COMMAND sort
      COMMAND md5
      COMMAND awk "{ print $1 }"
      OUTPUT_VARIABLE RESULT)
  endif()

  string(STRIP "${RESULT}" RESULT)

  set(${DIGEST}
      ${RESULT}
      PARENT_SCOPE)
endfunction()

function(ebp_add_library)
  set(OneValueArgs NAME DOWNLOAD_URL INSTALL_DIR CHKSUM)
  cmake_parse_arguments(EBP "" "${OneValueArgs}" "" "${ARGN}")
  if(EXISTS ${EBP_INSTALL_DIR} AND IS_DIRECTORY ${EBP_INSTALL_DIR})
    ebp_checksum_dir(${EBP_INSTALL_DIR} CHKSUM)
    # 如果 CHKSUM 参数是一个http连接，直接取http连接内容作为 CHKSUM 值
    if(${EBP_CHKSUM} MATCHES "^(http|https)://.*")
      execute_process(COMMAND /usr/bin/curl -sSf ${EBP_CHKSUM} OUTPUT_VARIABLE EBP_CHKSUM)
      string(STRIP "${EBP_CHKSUM}" EBP_CHKSUM)
    endif()
    string(STRIP "${CHKSUM}" CHKSUM)
    if("${CHKSUM}" STREQUAL "${EBP_CHKSUM}")
      set(EBP_OK YES)
    endif()
  endif()
  if(NOT EBP_OK)
    message("EBP&${EBP_NAME}: INSTALL_DIR: ${EBP_INSTALL_DIR} CHECKSUM '${CHKSUM}' != '${EBP_CHKSUM}'")
    # 预先删除不一致的文件夹
    string(TOLOWER ${EBP_NAME} EBP_NAME_LOWER)
    message(
      "EBP&${EBP_NAME}: delete file ${EBP_INSTALL_DIR}, ${CMAKE_BINARY_DIR}/_deps/${EBP_NAME_LOWER}-build, ${CMAKE_BINARY_DIR}/_deps/${EBP_NAME_LOWER}-subbuild"
    )
    if(POLICY CMP0135)
      cmake_policy(SET CMP0135 NEW)
    endif()
    file(REMOVE_RECURSE ${EBP_INSTALL_DIR})
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/_deps/${EBP_NAME_LOWER}-build)
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/_deps/${EBP_NAME_LOWER}-subbuild)
    FetchContent_Declare(${EBP_NAME} URL ${EBP_DOWNLOAD_URL} SOURCE_DIR ${EBP_INSTALL_DIR})
    FetchContent_Populate(${EBP_NAME})
  endif()
endfunction()
