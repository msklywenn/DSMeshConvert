#ifndef _COLOR_H_
#define _COLOR_H_

#include "types.h"

struct Color
{
	s32 r, g, b;

	Color() : r(0), g(0), b(0) {}
	Color(u16 c) { DecodePC555(c); }

	// Squared distance
	inline u32 Distance(const Color& c) const
	{
		int dr = c.r - r;
		int dg = c.g - g;
		int db = c.b - b;

		return dr * dr + dg * dg + db * db;
	}

	inline u16 EncodeNDS() const
	{
		u32 _r = r; if ( _r > 31 ) _r = 31;
		u32 _g = g; if ( _g > 31 ) _g = 31;
		u32 _b = b; if ( _b > 31 ) _b = 31;
		return _r | (_g << 5) | (_b << 10) | (1 << 15);
	}

	inline u16 EncodePC555() const
	{
		u32 _r = r; if ( _r > 31 ) _r = 31; if ( _r < 0 ) _r = 0;
		u32 _g = g; if ( _g > 31 ) _g = 31; if ( _g < 0 ) _g = 0;
		u32 _b = b; if ( _b > 31 ) _b = 31; if ( _b < 0 ) _b = 0;
		return (_r << 10) | (_g << 5) | _b;
	}

	inline void DecodePC555(u16 c)
	{
		r = (c >> 10) & 0x1F;
		g = (c >> 5) & 0x1F;
		b = c & 0x1F;
	}

	inline bool operator==(const Color& c) const
	{
		return c.r == r && c.g == g && c.b == b;
	}

	inline Color operator+(const Color& b)
	{
		Color c = *this;

		c.r += b.r;
		c.g += b.g;
		c.b += b.b;

		return c;
	}

	inline Color operator-(const Color& b)
	{
		Color c = *this;

		c.r -= b.r;
		c.g -= b.g;
		c.b -= b.b;

		return c;
	}

	inline Color operator*(s32 mul)
	{
		Color c = *this;

		c.r *= mul;
		c.g *= mul;
		c.b *= mul;

		return c;
	}

	inline Color operator/(s32 div)
	{
		Color c = *this;

		c.r /= div;
		c.g /= div;
		c.b /= div;

		return c;
	}
};

inline int CompareRed(const Color* a, const Color* b)
{
	return a->r - b->r;
}

inline int CompareGreen(const Color* a, const Color* b)
{
	return a->g - b->g;
}

inline int CompareBlue(const Color* a, const Color* b)
{
	return a->b - b->b;
}

inline void ColorCube(const Color& c, Color& min, Color& max)
{
	if ( c.r < min.r ) min.r = c.r;
	if ( c.g < min.g ) min.g = c.g;
	if ( c.b < min.b ) min.b = c.b;
	if ( c.r > max.r ) max.r = c.r;
	if ( c.g > max.g ) max.g = c.g;
	if ( c.b > max.b ) max.b = c.b;
}

#endif // _COLOR_H_