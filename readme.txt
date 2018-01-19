This little project implements a contour line tracing and vector extraction

Most interesting bit is found in function 'ExtractVectors'
For usage see 'singleFileTest'

Note: the "BlockMap" class and related code (pre-processing) is not quite necessary

Outline of algorithm:
1) Read an image to greyscale
2) Detect outline(s) based on Luma values -> ContourImage
3) Convert contourimage to a point cluster
4) Traverse the point cluster and create lines segments

Example usage:
1) Generate segments and render the segment image in one go
go run contour.go -m image.png segments.bin

2) Render segment file to image
go run contour.go -r segments.bin seg.png


Runing it with -h brings out help.
	Example: go run contour.go -h

Usage:
contour <options> input output
Input and Output can either be files or directories but not cominbed
PNG is only format supported for input images
Options
  g   Generate line segments from PNG save to Segment file/directory (default)
  r   Render image from segment file/directory and save as PNG file/directory
  v   Switch on extensive output
  ?/h This screen
Options for generating
  lcd <float>   Line Cutoff Distance, break condition for new segment, default: 10.000000
  lca <float>   Line Cutoff Angle, break condition for new segment, default: 0.500000
  lld <float>   Long Line Distance when searching for reference vector, default: 3.000000
  ccd <float>   Cluster Cutoff Distance, break condition for a new polygon, default: 50.000000