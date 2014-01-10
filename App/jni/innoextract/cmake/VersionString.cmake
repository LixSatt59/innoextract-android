
# Copyright (C) 2011-2013 Daniel Scharrer
#
# This software is provided 'as-is', without any express or implied
# warranty.  In no event will the author(s) be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

# Create a rule to generate a version string at compile time.
#
# An optional fifth argument can be used to add additional cmake defines.
#
# SRC is processed using the configure_file() cmake command
# at build to produce DST with the following variable available:
#
# VERSION_SOURCES:
#  List of (${var} ${file}) pairs.
#
# for each variable ${var}
# - ${var}: The contents of the associated file
# - ${var}_COUNT: Number of lines in the associated file
# - ${var}_${i}: The ${i}-th line of the associated file
# - ${var}_${i}_SHORTNAME: The first component of the ${i}-th line of the associated file
# - ${var}_${i}_STRING: Everything except the first component of the ${i}-th line of the associated file
# - ${var}_${i}_NAME: Everything except the last component of the ${i}-th line of the associated file
# - ${var}_${i}_NUMBER: The last component of the ${i}-th line of the associated file
# - ${var}_HEAD: The first paragraph of the associated file
# - ${var}_TAIL: The remaining paragraphs of the associated file
#
# - GIT_COMMIT: The current git commit. (not defined if there is no GIT_DIR directory)
# - GIT_COMMIT_PREFIX_${i}: The first ${i} characters of GIT_COMMIT (i=0..39)
# For the exact syntax of SRC see the documentation of the configure_file() cmake command.
# The version file is regenerated whenever VERSION_FILE or the current commit changes.
function(version_file SRC DST VERSION_SOURCES GIT_DIR)
	
	set(mode "variable")
	
	set(args)
	set(dependencies "${CMAKE_MODULE_PATH}/VersionScript.cmake")
	
	foreach(arg IN LISTS VERSION_SOURCES)
		
		if(mode STREQUAL "variable")
			set(mode "file")
		else()
			get_filename_component(arg "${arg}" ABSOLUTE)
			list(APPEND dependencies ${abs_file})
			set(mode "variable")
		endif()
		
		list(APPEND args ${arg})
		
	endforeach()
	
	get_filename_component(abs_src "${SRC}" ABSOLUTE)
	get_filename_component(abs_dst "${DST}" ABSOLUTE)
	get_filename_component(abs_git_dir "${GIT_DIR}" ABSOLUTE)
	
	set(defines)
	if(${ARGC} GREATER 4)
		set(defines ${ARGV4})
	endif()
	
	if(EXISTS "${abs_git_dir}/HEAD")
		list(APPEND dependencies "${abs_git_dir}/HEAD")
	endif()
	
	if(EXISTS "${abs_git_dir}/logs/HEAD")
		list(APPEND dependencies "${abs_git_dir}/logs/HEAD")
	endif()
	
	add_custom_command(
		OUTPUT
			"${abs_dst}"
		COMMAND
			${CMAKE_COMMAND}
			"-DINPUT=${abs_src}"
			"-DOUTPUT=${abs_dst}"
			"-DVERSION_SOURCES=${args}"
			"-DGIT_DIR=${abs_git_dir}"
			${defines}
			-P "${CMAKE_MODULE_PATH}/VersionScript.cmake"
		MAIN_DEPENDENCY
			"${abs_src}"
		DEPENDS
			${dependencies}
		COMMENT ""
		VERBATIM
	)
	
endfunction(version_file)
