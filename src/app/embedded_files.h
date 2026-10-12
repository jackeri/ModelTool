#pragma once

#include <cstddef>

namespace mt {

	/*! File built into the program (generated from the fonts folder by cmake/embed_files.cmake) */
	struct EmbeddedFile {
		const char *name = nullptr;			 ///< file name without the folder
		const unsigned char *data = nullptr; ///< file data, nullptr if there is no such file
		size_t size = 0;					 ///< data size in bytes
	};

	/**
	 * Find an embedded file
	 * @param name file name without the folder, like "Hack-Regular.ttf"
	 * @return the file, data is nullptr if it is not embedded
	 */
	EmbeddedFile embeddedFile(const char *name);
}
