#ifndef MODELTOOL_MT_H
#define MODELTOOL_MT_H

template <class X>
X& singleton()
{
	static X x;
	return x;
}

const int MAX_PATH = 1024;

typedef unsigned char byte;

#endif //MODELTOOL_MT_H
