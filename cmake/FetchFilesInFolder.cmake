function(MapFilesByPatternExcluding resultArray srcArray filterArray)
	set(result)

	foreach(item ${${srcArray}})
		set(needExclude FALSE)

		foreach(filterItem ${${filterArray}})
			if("${item}" STREQUAL "${filterItem}")
				set(needExclude TRUE)
				break()
			endif()
		endforeach()

		if(NOT needExclude)
			list(APPEND result "${item}")
		endif()
	endforeach()

	set(${resultArray} ${result} PARENT_SCOPE)
endfunction()

function(FetchFilesInFolder resultArray searchDirectory)
	set(allFiles)
	file(
		GLOB_RECURSE allFiles
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.c"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.cpp"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.h"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.asm"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.inl"
	)

	set(result)
	if(ARGN)
		MapFilesByPatternExcluding(result allFiles ${ARGN})
	else()
		set(result ${allFiles})
	endif()
	set(${resultArray} ${result} PARENT_SCOPE)
endfunction()