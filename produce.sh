
# this one is best so far
go run contour.go -m -cnt 4 -cns 2 -oca 0.97 -ccd 16 ~gnilk/Downloads/tmpout/ strips_0.97_ccd16_high_contrast.db 

# this one is missing out on some straight lines but produces otherwise nice stuff
go run contour.go -m -gl 128 -cnt 4 -cns 64 -lca 0.97 -oca 0.98 -ccd 16 -bs 16 ~gnilk/Downloads/tmpout/ strips_hq_ccd16_high_contrast.db 


# go run contour.go -w 255 -oca 0.5 ~gnilk/Downloads/tmpout/ strips_0.5.db
# go run contour.go -w 255 -oca 0.75 ~gnilk/Downloads/tmpout/ strips_0.75.db
# go run contour.go -w 255 -oca 0.90 ~gnilk/Downloads/tmpout/ strips_0.90.db
# go run contour.go -w 255 -oca 0.95 ~gnilk/Downloads/tmpout/ strips_0.95.db
# go run contour.go -w 255 -oca 0.97 ~gnilk/Downloads/tmpout/ strips_0.97.db

#go run contour.go -e -w 255 -oca 0.95 -datamode int8 ~gnilk/Downloads/tmpout/ segdir_opt_int8/
#go run contour.go -r -w 256 -h 256 -datamode int8 strips.db strip_images/
#ffmpeg -i strip_images/image_%d.png video_0.95_full.avi
