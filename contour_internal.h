#pragma once

namespace gnilk {
	namespace contour {

		class Point {
		public:
			int x,y;
		public:
			Point() {
				x = y = 0;
			}
			Point(int _x, int _y) {
				x = _x;
				y = _y;
			}
			int X() { return x; }
			int Y() { return y; }
			void Set(int _x, int _y) {
				x = _x;
				y = _y;
			}
			bool IsEqual(Point b) {
				if (b.X() != x ) return false;
				if (b.Y() != y ) return false;
				return true;
			}
		};

		class PointDistance {
		private:
			float distance;
			int pindex;
		public:
			PointDistance(float d, int idx) {
				distance = d;
				pindex = idx;
			}

			float Distance() { return distance; }
			int PIndex() { return pindex; }
			static bool Less(PointDistance *a, PointDistance *b) {
				return (a->distance < b->distance);
			}
		};

		typedef std::vector<Point> Strip;

	}
}