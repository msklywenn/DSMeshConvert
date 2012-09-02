#include <stdio.h>
#include <string>
#include "wingetopt.h"
#include "Compressor.h"

int main(int argc, char** argv)
{
	if ( argc < 3 )
	{
		static const char* msg = 
#if DITHERING
			"Usage: %s [-d] [-e] [-p preview] [-c palette_count] [-o output] <input>\n"
			"  -d : enable floyd-steinberg dithering\n"
#else
			"Usage: %s [-e] [-p preview] [-c palette_count] [-o output] <input>\n"
#endif
			"  -e : enable 2-colors extended palettes\n"
			"  -p <file> : output preview to file\n"
			"  -c <palette_count> : number of 4-colors palettes generated\n"
			"                       default is 192.\n"
			"  -o <output> : output file, default is input.ctx.\n";
		fprintf(stderr, msg, argv[0]);
		return 42;
	}

	std::string output;
	const char* preview = 0;
	u32 nbPalettes = 192;
	bool dither = false;
	bool extend = false;

	int c;
	while ( (c = getopt(argc, argv, "p:c:o:de")) != -1 )
	{
		switch ( c )
		{
			case 'p':
			{
				preview = optarg;
				break;
			}

			case 'c':
			{
				nbPalettes = atoi(optarg);
				break;
			}

			case 'o':
			{
				output = optarg;
				break;
			}

#if DITHERING
			case 'd':
			{
				dither = true;
				break;
			}
#endif

			case 'e':
			{
				extend = true;
				break;
			}
		}
	}

	const char* input = argv[optind];
	if ( output.size() == 0 )
	{
		output = input;
		output += ".ctx";
	}

	Compressor compressor;
	if ( ! compressor.Open(input) )
	{
		return 1;
	}
	
	compressor.Process(output.c_str(), nbPalettes, extend, dither, preview);

	return 0;
}