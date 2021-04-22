#ifndef MODELTOOL_MT_H
#define MODELTOOL_MT_H

template <class X>
X& singleton()
{
	static X x;
	return x;
}

typedef unsigned char byte;

#endif //MODELTOOL_MT_H
