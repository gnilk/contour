This little project implements a contour line tracing and vector extraction

Most interesting bit is found in function 'ExtractVectors'
For usage see 'singleFileTest'

Note: the "BlockMap" class and related code (pre-processing) is not quite necessary

Outline of algorithm:
1) Read an image to greyscale
2) Detect outline(s) based on Luma values -> ContourImage
3) Convert contourimage to a point cluster
4) Traverse the point cluster and create lines segments
