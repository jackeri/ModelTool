#include "binary_reader.h"

namespace mt::io {

	BinaryReader::BinaryReader(const Ref<MTFile> &file)
	{
		if (!file || !file->valid())
		{
			throw mt_ex("Cannot read an empty file");
		}

		owner = file->data;
		data = owner->data();
		length = file->len ? std::min(file->len, owner->size()) : owner->size();
		name = file->name;
	}

	BinaryReader::BinaryReader(const byte *data, size_t size, std::string name) : data(data), length(size), name(std::move(name)) {}

	bool BinaryReader::canRead(int64_t start, int64_t bytes) const
	{
		return start >= 0 && bytes >= 0 && static_cast<uint64_t>(start) <= length
			   && static_cast<uint64_t>(bytes) <= length - static_cast<uint64_t>(start);
	}

	void BinaryReader::seek(int64_t position)
	{
		check(position, 0);
		offset = static_cast<size_t>(position);
	}

	void BinaryReader::skip(int64_t bytes)
	{
		check(static_cast<int64_t>(offset), bytes);
		offset += static_cast<size_t>(bytes);
	}

	std::string BinaryReader::readString(int64_t fieldSize)
	{
		check(static_cast<int64_t>(offset), fieldSize);
		const char *start = reinterpret_cast<const char *>(data + offset);
		std::string value(start, strnlen(start, static_cast<size_t>(fieldSize)));
		offset += static_cast<size_t>(fieldSize);
		return value;
	}

	BinaryReader BinaryReader::sub(int64_t start, int64_t bytes) const
	{
		check(start, bytes);
		BinaryReader reader(data + start, static_cast<size_t>(bytes), name);
		reader.owner = owner;
		return reader;
	}

	BinaryReader BinaryReader::from(int64_t start) const
	{
		check(start, 0);
		return sub(start, static_cast<int64_t>(length) - start);
	}

	void BinaryReader::check(int64_t start, int64_t bytes) const
	{
		if (!canRead(start, bytes))
		{
			fail("Out of bounds read of " + std::to_string(bytes) + " bytes at offset " + std::to_string(start) + " (data size "
				 + std::to_string(length) + ")");
		}
	}

	void BinaryReader::fail(const std::string &message) const
	{
		throw mt_ex(name.empty() ? message : name + ": " + message);
	}
}
