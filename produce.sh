go run contour.go -e -w 255 -oca 0.95 -datamode int8 ~gnilk/Downloads/tmpout/ segdir_opt_int8/
go run contour.go -r -w 256 -h 256 -datamode int8 strips.db strip_images/
ffmpeg -i strip_images/image_%d.png video_0.95_full.avi
