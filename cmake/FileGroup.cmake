# generate file group follows the struct of file system
function(GOURP_FILES folder)
	file(GLOB_RECURSE source_files RELATIVE "${folder}" CONFIGURE_DEPENDS *.hpp *.cpp *.hxx *.cxx *.h *.c)
	
	list(APPEND header_exts .hpp .hxx .h)
	list(APPEND source_exts .cpp .cxx .c)

	foreach(file ${source_files})
		# get file last ext
		get_filename_component(last_ext "${file}" LAST_EXT)
		if ("${last_ext}" IN_LIST header_exts)
			set(prefix "Header Files")
		elseif("${last_ext}" IN_LIST source_exts)
			set(prefix "Source Files")
		else()
			set(prefix "Other Files")
		endif()

		# get file relative path
		get_filename_component(absolute_path "${file}" ABSOLUTE)
		get_filename_component(parent_dir "${absolute_path}" DIRECTORY)
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" group "${parent_dir}")
		string(REPLACE "/" "\\" group "${group}")
		set(group "${prefix}${group}")
		source_group("${group}" FILES "${file}")
	endforeach()
endfunction()