#pragma once

#include "library.h"

#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

namespace mt::io {
	/*! Loaded file instance */
	class MTFile {
	  public:
		MTFile() = default;

		~MTFile() = default;

		/**
		 * Checks if the loaded data is valid and usable
		 * @return data validity
		 */
		[[nodiscard]] bool valid() const
		{
			return (data && !data->empty());
		}

		/**
		 * Returns the loaded data as a C -string
		 * @return char array pointer
		 */
		[[nodiscard]] const char *c_str() const
		{
			if (valid())
			{
				return reinterpret_cast<char *>(data->data());
			}

			return "";
		}

		/**
		 * Returns the data as a stl string
		 * @return std::string instance
		 */
		[[nodiscard]] std::string string() const
		{
			return {c_str()};
		}

		explicit operator const char *() const
		{
			return c_str();
		}

		explicit operator std::string() const
		{
			return string();
		}

		byte_buffer data = nullptr; ///< Loaded file data
		std::string name;			///< Full name of the file
		std::string ext;
		size_t len = 0;
	};

	struct FileRecord;
	using FileList = RefList<FileRecord>;

	/*! Base class for different file sources like a local filesystem path, or a zip file */
	class FileSource {
	  public:
		FileSource()
		{
			this->identifier = tools::randomString(64);
		}

		virtual ~FileSource() = default;

		/**
		 * Checks if a file is found in this source
		 * @param name relative path to a file
		 * @return is the file found
		 */
		virtual bool findFile(const std::string &name) = 0;

		/**
		 * Reads and loads a file to a MTFile instance buffer
		 * @param name relative path to a file
		 * @return MTFile instance
		 */
		virtual Ref<MTFile> loadFile(const std::string &name) = 0;

		/**
		 * Get files in the root of the source
		 * @return list of files
		 */
		virtual FileList getFiles()
		{
			return getFiles("");
		}

		/**
		 * Get files in the relative path
		 * @param name folder path to use
		 * @return list of files
		 */
		virtual FileList getFiles(const std::string &name) = 0;

		/**
		 * Get the path of the source (folder or a zip file for example)
		 * @return path of the source
		 */
		[[nodiscard]] virtual std::string getSource() const = 0;

		/**
		 * Returns an unique generated identifier for the file source
		 * @return
		 */
		[[nodiscard]] std::string getIdentifier() const
		{
			return identifier;
		}

	  private:
		std::string identifier; ///< Unique generated identifier for this file source
	};

	/*! Record of a file found in a FileSource's path */
	struct FileRecord {
		std::string path{};			  ///< Relative path of the file
		std::string name{};			  ///< Name of the file
		std::string ext{};			  ///< Extension of the file
		bool isDirectory = false;	  ///< Is the file a directory
		FileSource *source = nullptr; ///< Source of this record
	};
}
