#
# gather_include_directories recursively builds a list of include directories
# across all dependencies.
#

function(_hsh_gather_include_directories_impl target_name)
  get_target_property(target_dependencies ${target_name} INTERFACE_LINK_LIBRARIES)
  foreach(dep ${target_dependencies})
    if(TARGET ${dep})
      get_target_property(dep_includes ${dep} INTERFACE_INCLUDE_DIRECTORIES)
      if(dep_includes)
        list(APPEND target_includes ${dep_includes})
      endif()
      _hsh_gather_include_directories_impl(${dep})
    endif()
  endforeach()
  set(target_includes ${target_includes} PARENT_SCOPE)
endfunction()

function(hsh_gather_include_directories var target_name)
  unset(target_includes)
  get_directory_property(dir_includes INCLUDE_DIRECTORIES)
  if(dir_includes)
    list(APPEND target_includes ${dir_includes})
  endif()
  get_target_property(target_includes1 ${target_name} INCLUDE_DIRECTORIES)
  if(target_includes1)
    list(APPEND target_includes ${target_includes1})
  endif()
  get_target_property(target_includes2 ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
  if(target_includes2)
    list(APPEND target_includes ${target_includes2})
  endif()
  _hsh_gather_include_directories_impl(${target_name})
  list(REMOVE_DUPLICATES target_includes)
  set(${var} ${target_includes} PARENT_SCOPE)
endfunction()

function(_hsh_gather_compile_definitions_impl target_name)
  get_target_property(target_dependencies ${target_name} INTERFACE_LINK_LIBRARIES)
  foreach(dep ${target_dependencies})
    if(TARGET ${dep})
      get_target_property(dep_includes ${dep} INTERFACE_COMPILE_DEFINITIONS)
      if(dep_includes)
        list(APPEND target_includes ${dep_includes})
      endif()
      _hsh_gather_include_directories_impl(${dep})
    endif()
  endforeach()
  set(target_includes ${target_includes} PARENT_SCOPE)
endfunction()

function(hsh_gather_compile_definitions var target_name)
  unset(target_includes)
  get_directory_property(dir_includes COMPILE_DEFINITIONS)
  if(dir_includes)
    list(APPEND target_includes ${dir_includes})
  endif()
  get_target_property(target_includes1 ${target_name} COMPILE_DEFINITIONS)
  if(target_includes1)
    list(APPEND target_includes ${target_includes1})
  endif()
  get_target_property(target_includes2 ${target_name} INTERFACE_COMPILE_DEFINITIONS)
  if(target_includes2)
    list(APPEND target_includes ${target_includes2})
  endif()
  _hsh_gather_include_directories_impl(${target_name})
  list(REMOVE_DUPLICATES target_includes)
  set(${var} ${target_includes} PARENT_SCOPE)
endfunction()

#
# target_hsh makes all sources of a target individually depend on
# its own generated hshhead file.
#
function(target_hsh target)
  cmake_parse_arguments(ARG
          ""
          ""
          "HSH_TARGETS"
          ${ARGN})
  unset(_hsh_args)
  foreach(target ${HSH_TARGETS})
    list(APPEND _hsh_args "--${target}")
  endforeach()
  foreach(target ${ARG_HSH_TARGETS})
    list(APPEND _hsh_args "--${target}")
  endforeach()
  list(REMOVE_DUPLICATES _hsh_targets)
  get_target_property(bin_dir ${target} BINARY_DIR)
  get_target_property(src_dir ${target} SOURCE_DIR)
  get_target_property(source_list ${target} SOURCES)
  target_include_directories(${target} PRIVATE "${HSH_INCLUDE_DIR}")
  hsh_gather_include_directories(include_list ${target})
  foreach(include ${include_list})
    list(APPEND _hsh_args "-I${include}")
  endforeach()
  hsh_gather_compile_definitions(define_list ${target})
  foreach(define ${define_list})
    list(APPEND _hsh_args "-D${define}")
  endforeach()
  foreach(source ${source_list})
    get_source_file_property(src_path "${source}" LOCATION)
    file(RELATIVE_PATH rel_path "${src_dir}" "${src_path}")
    get_filename_component(rel_dir "${rel_path}" DIRECTORY)
    if (rel_dir)
      set(rel_dir "${rel_dir}/")
    endif()
    string(REPLACE ".." "__" rel_dir "${rel_dir}")
    set(out_dir "HshFiles/${rel_dir}")
    set(out_path "${bin_dir}/${out_dir}${rel_path}.hshhead")
    file(MAKE_DIRECTORY "${bin_dir}/${out_dir}")
    file(RELATIVE_PATH out_rel ${CMAKE_BINARY_DIR} "${out_path}")
    unset(depfile_args)
    if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
      list(APPEND depfile_args DEPFILE "${CMAKE_BINARY_DIR}/${out_rel}.d")
    endif()
    add_custom_command(OUTPUT "${out_path}" COMMAND "$<TARGET_FILE:hshgen>"
            ARGS ${_hsh_args} "${src_path}" "${out_rel}"
            -MD -MT "${out_rel}" -MF "${out_rel}.d"
            DEPENDS hshgen "${src_path}" IMPLICIT_DEPENDS CXX "${src_path}"
            ${depfile_args} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    set_source_files_properties("${source}" PROPERTIES
            INCLUDE_DIRECTORIES "${bin_dir}/${out_dir}"
            OBJECT_DEPENDS "${out_path}"
            COMPILE_FLAGS -Wno-unknown-attributes)
    target_sources(${target} PUBLIC "${out_path}")
  endforeach()
endfunction()
