#ifndef MODELTOOL_MT_H
#define MODELTOOL_MT_H

#include <vector>

template <class X>
X& singleton()
{
	static X x;
	return x;
}

const int MAX_PATH = 1024;

// using byte = unsigned char;
// using byte_buffer = std::shared_ptr<std::vector<unsigned char>>;

typedef unsigned char byte;
typedef std::shared_ptr<std::vector<byte>> byte_buffer;

inline byte_buffer buffer(size_t size)
{
	return std::make_shared<std::vector<byte>>(size);
}

#endif //MODELTOOL_MT_H
