function(FetchFilesInFolder resultArray searchDirectory)
	file(
		GLOB_RECURSE result
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.c"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.cpp"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.h"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.asm"
			"${CMAKE_CURRENT_SOURCE_DIR}/${searchDirectory}/*.inl"
	)
	set(${resultArray} ${result} PARENT_SCOPE)
endfunction()