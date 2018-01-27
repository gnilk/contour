#pragma once

#include <string>

namespace gnilk {
	class Bitmap {
	private:
		int width;
		int height;
		unsigned char *buffer;
	public:
		Bitmap(int w, int h);
		Bitmap(int w, int h, unsigned char *buffer);
		virtual ~Bitmap();	

		static Bitmap *FromAlpha(int w, int h, unsigned char *srcData);
		static Bitmap *FromRGBA(int w, int h, unsigned char *srcData);
		static Bitmap *LoadPNGImage(std::string imagefile);
		static Bitmap *LoadPNGImage(void *buffer, long numbytes);


		void CreateAlphaMask(unsigned char alpha_min, unsigned char alpha_max);
	 	void SetAlpha(unsigned char value);
		int Width() { return width; }
		int Height() { return height; }
		unsigned char *Buffer() { return buffer; }
		unsigned char *Buffer(int x, int y);

		Bitmap *CopyToNew(int x, int y, int w, int h);
	};


}
