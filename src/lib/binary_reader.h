#pragma once

#include "library.h"
#include "files.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "BinaryReader assumes a little-endian host, model files are little-endian"
#endif

namespace mt::io {

	/*! Bounds-checked reader for little-endian binary model files. Every read throws if it would go past the data. */
	class BinaryReader {
	  public:
		/**
		 * Reader over a loaded file
		 * @param file loaded file, the length is taken from MTFile::len (or the buffer size if len is not set)
		 */
		explicit BinaryReader(const Ref<MTFile> &file);

		/**
		 * Reader over a raw memory block, the memory needs to outlive the reader
		 * @param data pointer to the data
		 * @param size size of the data in bytes
		 * @param name name used in error messages
		 */
		BinaryReader(const byte *data, size_t size, std::string name = "");

		/**
		 * @return size of the readable data in bytes
		 */
		[[nodiscard]] size_t size() const
		{
			return length;
		}

		/**
		 * @return current read offset
		 */
		[[nodiscard]] size_t tell() const
		{
			return offset;
		}

		/**
		 * @return number of bytes left after the current offset
		 */
		[[nodiscard]] size_t remaining() const
		{
			return length - offset;
		}

		/**
		 * Check if a block can be read without going out of bounds
		 * @param start offset of the block
		 * @param bytes size of the block
		 * @return true if the whole block is inside the data
		 */
		[[nodiscard]] bool canRead(int64_t start, int64_t bytes) const;

		/**
		 * Move the read offset
		 * @param position absolute offset, can be equal to size() (end of data)
		 */
		void seek(int64_t position);

		/**
		 * Move the read offset forward
		 * @param bytes number of bytes to skip
		 */
		void skip(int64_t bytes);

		/**
		 * Read a value and advance the offset
		 * @tparam T trivially copyable type (plain file format struct or a number)
		 * @return read value
		 */
		template<typename T>
		T read()
		{
			T value = readAt<T>(static_cast<int64_t>(offset));
			offset += sizeof(T);
			return value;
		}

		/**
		 * Read a value from an absolute offset without moving the read offset
		 * @tparam T trivially copyable type (plain file format struct or a number)
		 * @param position absolute offset
		 * @return read value
		 */
		template<typename T>
		[[nodiscard]] T readAt(int64_t position) const
		{
			static_assert(std::is_trivially_copyable_v<T>, "BinaryReader can only read trivially copyable types");

			check(position, sizeof(T));
			T value;
			std::memcpy(&value, data + position, sizeof(T));
			return value;
		}

		/**
		 * Read an array of values and advance the offset
		 * @tparam T trivially copyable type
		 * @param count number of elements, negative counts throw
		 * @return vector of read values
		 */
		template<typename T>
		std::vector<T> readArray(int64_t count)
		{
			static_assert(std::is_trivially_copyable_v<T>, "BinaryReader can only read trivially copyable types");

			if (count < 0)
			{
				fail("Negative element count " + std::to_string(count));
			}

			if (static_cast<uint64_t>(count) > remaining() / sizeof(T))
			{
				fail("Element count " + std::to_string(count) + " does not fit in the remaining " + std::to_string(remaining()) + " bytes");
			}

			std::vector<T> values(static_cast<size_t>(count));
			if (count > 0)
			{
				std::memcpy(values.data(), data + offset, sizeof(T) * values.size());
			}
			offset += sizeof(T) * values.size();
			return values;
		}

		/**
		 * Read a fixed size character field (like char name[64]) and advance the offset
		 * @param fieldSize size of the field in bytes
		 * @return the string up to the first null character
		 */
		std::string readString(int64_t fieldSize);

		/**
		 * Turn a fixed size character array from a file format struct into a string
		 * @tparam N size of the array
		 * @param field the character array
		 * @return the string up to the first null character
		 */
		template<size_t N>
		static std::string toString(const char (&field)[N])
		{
			return {field, strnlen(field, N)};
		}

		/**
		 * Create a reader over a block of this data, offsets in the new reader start from 0
		 * @param start offset of the block
		 * @param bytes size of the block
		 * @return reader over the block
		 */
		[[nodiscard]] BinaryReader sub(int64_t start, int64_t bytes) const;

		/**
		 * Create a reader from an offset to the end of this data
		 * @param start offset of the block
		 * @return reader over the rest of the data
		 */
		[[nodiscard]] BinaryReader from(int64_t start) const;

	  private:
		void check(int64_t start, int64_t bytes) const;

		[[noreturn]] void fail(const std::string &message) const;

		byte_buffer owner = nullptr; ///< keeps the file data alive when reading a file
		const byte *data = nullptr;	 ///< start of the readable data
		size_t length = 0;			 ///< size of the readable data
		size_t offset = 0;			 ///< current read offset
		std::string name;			 ///< file name for error messages
	};
}
