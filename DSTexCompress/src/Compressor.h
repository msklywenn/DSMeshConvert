#ifndef _COMPRESSOR_H_
#define _COMPRESSOR_H_

#include <FreeImagePlus.h>
#include "types.h"

class Compressor
{
	fipImage image;

public:
	bool Open(const char* input);
	void Process(const char* output, u32 nbPalettes, bool extend, bool dither, const char* preview);
};

#endif // _COMPRESSOR_H_