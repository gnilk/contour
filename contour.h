#pragma once

#include <math.h>
#include <map>
#include <vector>

#include "bitmap.h"
#include "vec2d.h"
#include "contour_internal.h"

namespace gnilk {
	namespace contour {
		class Block;
		class BlockMap;


		struct Config {
			uint8_t GreyThresholdLevel;
			float ClusterCutOffDistance;
			float LineCutOffDistance;
			float LineCutOffAngle;
			float LongLineDistance;
			float OptimizationCutOffAngle;
			float ContrastFactor; // NOT USED
			float ContrastScale; // NOT USED
			int BlockSize;
			float FilledBlockLevel;
			int Width;
			int Height;
			bool Optimize;
			bool Verbose;
		};


		class LineSegment {
		private:
			Point ptStart;
			Point ptEnd;
			int idxStart;
			int idxEnd;
		public:
			LineSegment(Point _start, Point _end) {
				ptStart = _start;
				ptEnd = _end;
				idxStart = -1;
				idxEnd = -1;
			}
			LineSegment(Point _start, Point _end, int iStart, int iEnd) {
				ptStart = _start;
				ptEnd = _end;
				idxStart = iStart;
				idxEnd = iEnd;
			}

			Point Start() {
				return ptStart;
			}
			Point End() {
				return ptEnd;
			}
			Point *StartPtr() {
				return &ptStart;
			}
			Point *EndPtr() {
				return &ptEnd;
			}
			int IdxEnd() {
				return idxEnd;				
			}
			int IdxStart() {
				return idxStart;
			}

			void SetStart(int x, int y) {
				ptStart.Set(x,y);
			}
			void SetEnd(int x, int y) {
				ptEnd.Set(x,y);
			}

			Vec2D *AsVector(Vec2D *dst) {
				dst->x = (float)(ptEnd.x - ptStart.x);	
				dst->y = (float)(ptEnd.y - ptStart.y);	
				return dst;
			}
			float Len() {
				Vec2D tmp;
				return AsVector(&tmp)->Abs();	
			}
		};

		class ContourPoint {
		private:
			int pindex;
			Point pt;
			Block *block;
			bool used;
		public:
			ContourPoint(int x, int y, Block *b);

			int X() { return pt.x; }
			int Y() { return pt.y; }
			Point Pt() { return pt; }
			bool IsUsed() { return used; }
			void Use() { used = true; }
			void ResetUsage() { used = false; }
			int PIndex() { return pindex; }
			void SetPIndex(int idx) { pindex = idx; }
			Block *GetBlock() { return block; }
			float Distance(ContourPoint *other);
		};

		class ContourCluster {
		private:
			std::vector<ContourPoint *> &points;
			BlockMap *blockmap;
		public:
			ContourCluster(std::vector<ContourPoint *> &points, BlockMap *map);
			std::vector<LineSegment *> ExtractVectors();
		private:
			LineSegment *NextSegment(int idxStart);						
			void CalcPointDistance(int pidx, std::vector<PointDistance *> &distances);
			void LocalSearchPointDistance(int pidx, std::vector<PointDistance *> &distances);
			void FullSearchPointDistance(int pidx, std::vector<PointDistance *> &distances);
			LineSegment *NewLineSegment(int idxA, int idxB);
			int Len() { return points.size(); }
			ContourPoint *At(int idx) { return points[idx]; }
			Vec2D *NewVector(int idxA, int idxB);
		};


		class Block {
		private:
			Bitmap *bitmap;
			int x,y;
			bool visited;	// during scan
			bool extracted;	// during line extraction
		public:
			std::vector<ContourPoint *> points;


		private:
			int HashFunc(int x, int y);
			uint8_t ReadGreyPixel(int x, int y);
			float PixelAsFloat(int x, int y);
		public:
			Block(Bitmap *bitmap, int x, int y);
			int Hash();
			int Left();
			int Right();
			int Up();
			int Down();

			void Scan(std::vector<ContourPoint *> &pnts);
			void CalcPointDistance(ContourPoint *cp, std::vector<PointDistance *> &distances);

			bool IsVisited() { return visited; }
			void Visit() { visited = true; }

			bool IsExtracted() { return extracted; }
			void SetExtracted() { extracted = true; }

			int NumPoints() { return points.size(); }
		};

		class BlockMap {
		private:
			Bitmap *bitmap;
			std::map<int, Block *> blocks;

			void BuildBlocks();
		public:
			BlockMap(Bitmap *bitmap);
			Block *GetBlock(int hashValue);
			Block *Left(Block *block);
			Block *Right(Block *block);
			Block *Up(Block *block);
			Block *Down(Block *block);
			Block *GetBlockForExtraction(Block *previous = NULL);
			void Scan(std::vector<ContourPoint *> &points, Block *b);
			int NumBlocks() { return blocks.size(); }
			std::vector<ContourPoint *> ExtractContourPoints();
		private:
			Block *GetBlockForExtractionRecursive(Block *previous);
		};



		class Trace {
		private:
			int intermediateWidth;
			int intermediateHeight;
			void SetDefaultConfig();
		public:
			Trace();
			void ProcessImage(unsigned char *data, int width, int height);
		private:
			void OptimizeLineSegments(std::vector<LineSegment *> &newSegments, std::vector<LineSegment *> &lineSegments);
			void RescaleLineSegments(std::vector<LineSegment *> &lineSegments, int w, int h);
			int LineSegmentsToStrips(std::vector<Strip *> &strips, std::vector<LineSegment *> &lineSegments);
			void DumpStrips(const char *title, std::vector<Strip *> &strips);
			void WriteStrips(std::string filename, std::vector<Strip *> &strips);
			Bitmap *DrawLineSegments(std::vector<LineSegment *> &lineSegments);
			Bitmap *DrawCluster(std::vector<ContourPoint *> &points);
			void DrawLine(Bitmap *dst, Point a, Point b, uint8_t cr, uint8_t cg, uint8_t cb);
		};

	}
}