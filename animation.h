#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <vector>


namespace gnilk {

	struct AnimationRenderVars {
		int idxFrame;
		int numStrips;		
		bool highLightFirstInStrip;
		bool highLightLineInStrip;
		int idxStrip;
		int idxLineInStrip;
	};

	struct Point {
		uint8_t x;
		uint8_t y;
		float fx;	// Normalized range (-1 .. 1
		float fy;	// Normalized range (-1 .. 1)
	};

	class Strip {
	private:
		uint8_t nPoints;
		std::vector<Point> points;
	private:
		void Normalize();
	public:
		void Load(FILE *f);
		int Points() { return points.size(); }
		void Render(bool highlight = false);
		float Len(int idxStart, int idxEnd);
		float FixLen(int idxStart, int idxEnd);
	};

	class Frame {
	private:
		uint8_t nStrips;
		std::vector<Strip> strips;
	private:
	public:
		void Load(FILE *f);
		int Strips() { return strips.size(); }
		Strip At(int index) { return strips[index]; }
		void Render();
		void Dump();
	};

	class Animation {
	private:
		std::vector<Frame *> frames;
	public:
		void LoadFromFile(const char *filename);
		int Frames() { return frames.size(); }
		void Render(int frame, AnimationRenderVars vars);	
		Frame *At(int index) { return frames[index]; }
		int MaxStrips();
	};

}
