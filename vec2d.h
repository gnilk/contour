#pragma once

#include <math.h>
#include "contour_internal.h"

namespace gnilk {
	namespace contour {
		class Vec2D {
		public:
			float x;
			float y;
		public:
			Vec2D() {
				
			}
			Vec2D(float _x, float _y) {
				x = _x;
				y = _y;
			}
			Vec2D(Point a, Point b) {
				x = (float)(b.X() - a.X());
				y = (float)(b.Y() - a.Y());
			}
			Vec2D(Point p) {
				x = (float)(p.X());
				y = (float)(p.Y());
			}
			Vec2D *Dup() {
				return new Vec2D(x,y);
			}
			Vec2D *Add(Vec2D *other) {
				x += other->x;
				y += other->y;
				return this;
			}
			Vec2D *Sub(Vec2D *other) {
				x -= other->x;
				y -= other->y;
				return this;
			}
			Vec2D *Neg() {
				x *= -1.0f;
				y *= -1.0f;
				return this;
			}
			float Dot(Vec2D *other) {
				return (x * other->x + y * other->y);
			}
			float Abs() {
				return sqrt(x*x + y*y);
			}
			Vec2D *Norm() {
				float oneoverl = 1.0 / Abs();
				x *= oneoverl;
				y *= oneoverl;
				return this;
			}
		};
	}
}