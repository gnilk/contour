go run contour.go -w 255 -oca 0.5 ~gnilk/Downloads/tmpout/ strips_0.5.db
go run contour.go -w 255 -oca 0.75 ~gnilk/Downloads/tmpout/ strips_0.75.db
go run contour.go -w 255 -oca 0.90 ~gnilk/Downloads/tmpout/ strips_0.90.db
go run contour.go -w 255 -oca 0.95 ~gnilk/Downloads/tmpout/ strips_0.95.db
go run contour.go -w 255 -oca 0.97 ~gnilk/Downloads/tmpout/ strips_0.97.db

#go run contour.go -e -w 255 -oca 0.95 -datamode int8 ~gnilk/Downloads/tmpout/ segdir_opt_int8/
#go run contour.go -r -w 256 -h 256 -datamode int8 strips.db strip_images/
#ffmpeg -i strip_images/image_%d.png video_0.95_full.avi
