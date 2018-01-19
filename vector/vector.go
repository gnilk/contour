package vector

import (
	"image"
	"math"
)

type Vec2D struct {
	X float64
	Y float64
}

func NewVec2D(x, y float64) Vec2D {
	v := Vec2D{
		X: x,
		Y: y,
	}
	return v
}

func NewVec2DFromPoint(a image.Point) Vec2D {
	return NewVec2D(float64(a.X), float64(a.Y))
}

func NewVec2DFromPoints(a, b image.Point) Vec2D {
	v := Vec2D{
		X: float64(b.X) - float64(a.X),
		Y: float64(b.Y) - float64(a.Y),
	}
	return v
}

func (v *Vec2D) Dup() Vec2D {
	vNew := Vec2D{
		X: v.X,
		Y: v.Y,
	}
	return vNew
}

func (v *Vec2D) Add(other *Vec2D) *Vec2D {
	v.X = v.X + other.X
	v.Y = v.Y + other.Y

	return v
}

func (v *Vec2D) Sub(other *Vec2D) *Vec2D {
	v.X = v.X - other.X
	v.Y = v.Y - other.X
	return v
}

func (v *Vec2D) Neg() *Vec2D {
	v.X = v.X * -1
	v.Y = v.Y * -1
	return v
}

func (v *Vec2D) Dot(other *Vec2D) float64 {
	return (v.X*other.X + v.Y*other.Y)
}

func (v *Vec2D) Abs() float64 {
	return math.Sqrt(v.X*v.X + v.Y*v.Y)
}

func (v *Vec2D) Norm() *Vec2D {
	oneoverl := 1.0 / v.Abs()
	v.X = v.X * oneoverl
	v.Y = v.Y * oneoverl
	return v
}
