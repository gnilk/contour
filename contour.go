package main

//
// Contour line tracing and vector extraction
//
// Most interesting bit is found in function 'ExtractVectors'
// For usage see 'singleFileTest'
//
// Note: the "BlockMap" class and related code (pre-processing) is not quite necessary
//
// Outline of algorithm:
// 1) Read an image to greyscale
// 2) Increase contrast (per pixel) - set both factors to '1' to disabled
//        v = pow(v, contrast_factor) * contrast_scale
// 2) Detect outline based on Luma values -> ContourImage
//        control threshold with gl - greyscale level
// 3) Convert contourimage to a point cluster
//
// 4) Traverse the point cluster and create lines segments
//
// 5) Optimize line segments by merging adjacent with small deviation
//
// Default Optimization is local range search
//          1) Track the block a point belongs to in the 'func (b* Block)Scan'
//             - This requires replacing the 'image.Point' with something else
//          2) In the ExtractVector 'calculatedistance' function only search current+neigbouring blocks
//
//          This should limit the amount of pixels we need to touch!
// Use '-F' to switch on FullRangeSearch (all pixels) - complicated pictures will take > 10sec to process while local search take <1sec
//
// Stats ('Bad Apple') - Execution time (6573 frames)
//	-F -oca 0.95, 4496 seconds => 1.44 FPS
//	-oca 0.95,  2107 seconds => 3.07 FPS
//
//
// TODO:
//
// Cheat Sheet for creating the line-segment video:
//
// Extract PNG from Video
//   ffmpeg -i CoolMusicVideo.avi tmpout/images%d.png
//
// Encode multiple PNG to Video:
// 	ffmpeg -i seq_cont_%d.png video.avi
//
// Extract music from video:
//  ffmpeg -i ~gnilk/Downloads/CoolMusicVideo.avi -f mp3 -vn music.mp3
//
// Mix video with encoded PNG's
//   ffmpeg -i video.avi -i music.mp3 -codec copy -shortest mixed.avi
//
// Examples on how to use this:
//   # Generate vectors from image.png save to image.seg and rescale to 255 pixels maintain aspect
//   go run contour.go -g -w 255 image.png image.seg
//
//   # As above but override aspect calculation
//   go run contour.go -g -w 255 -h 255 image.png image.seg
//
//   # Render segments to image (image.seg.png), image size is 256x256
//   go run contour.go -r -w 256 -h 256 image.seg image.seg.png
//
//   # generate segments/vectors from all files in imagefiles save in 'segmentfiles'
//   go run contour.go -g imagefiles/ segmentfiles/
//
//   # generate images for all segments files in 'segmentfiles' save in 'segimages'
//   go run contour.go -r -w 256 -h 256 segmentfiles/ segimages/
//
//
//   Full script to convert a video
//   1) Convert video to frames
//   	ffmpeg -i video.avi images/image%d.png
//
//   2) Extract vector segments
//   	go run contour.go -g images/ segments/
//
//	 3) Generate images from vector segments
//   	go run contour.go -r -w 1920 -h 1080 segments/ segimages/
//
//	 4) Generate AVI from images
//   	ffmpeg -i segimages/image%d.png.seg.png video.avi
//
//

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"log"
	"math"
	"os"
	"path"
	"sort"
	"strconv"
	"time"

	vector "contour/vector"
)

type OpMode int64

const (
	_                     = iota
	OpModeGenerate OpMode = 10
	OpModeRender   OpMode = 20
)

type DataMode int64

const (
	_                      = iota
	DataModeInt8  DataMode = 10
	DataModeInt16 DataMode = 20
)

type Config struct {
	GreyThresholdLevel      uint8
	ClusterCutOffDistance   float64
	LineCutOffDistance      float64
	LineCutOffAngle         float64
	LongLineDistance        float64
	OptimizationCutOffAngle float64
	ContrastFactor          float64
	ContrastScale           float64
	BlockSize               int
	FilledBlockLevel        float32
	Width                   int
	Height                  int
	// Options
	Verbose                  bool
	Mode                     OpMode
	Input                    string
	Output                   string
	GenerateIntermediateData bool
	ListVectors              bool
	Optimize                 bool
	Rescale                  bool
	DataMode                 DataMode
	SaveEmptySegmentFile     bool
	OptimizedRangeSearch     bool
	NumFrames                int
}

var glbConfig = Config{
	GreyThresholdLevel:      32,   // This is the contour threshold value
	ClusterCutOffDistance:   50,   // This is when a point is considered to be a new cluster
	LineCutOffDistance:      10.0, // Creates new segment when distance is too far also not dependent on the distance vector in case of sparse clusters
	LineCutOffAngle:         0.5,  // Creates a new segment when deviating angle hits this threshold, only comes in play when we can derive a directional vector (see: LongLineDistance)
	LongLineDistance:        3.0,  // Distance to search before capturing the directional vector for the segment
	BlockSize:               8,    // Not used
	FilledBlockLevel:        0.5,  // Not used
	OptimizationCutOffAngle: 0.95,
	ContrastFactor:          4,
	ContrastScale:           2,
	Width:                   255,
	Height:                  0,
	// Options
	Verbose: false, // Switch on heavy debug output
	Mode:    OpModeGenerate,
	Input:   "",
	Output:  "",
	GenerateIntermediateData: false,
	ListVectors:              false,
	Optimize:                 true,
	Rescale:                  true,
	DataMode:                 DataModeInt8,
	SaveEmptySegmentFile:     false,
	OptimizedRangeSearch:     true,
	NumFrames:                0,
}

type BlockMap map[int]*Block

func main() {

	log.SetOutput(os.Stdout)
	parseOptions()

	if glbConfig.Input == "" {
		log.Fatal("Missing input file/directory")
	}
	if glbConfig.Output == "" {
		log.Fatal("Missing output file/directory")
	}

	tStart := time.Now()

	if glbConfig.Mode == OpModeGenerate {
		generateDataFromConfig()
	} else if glbConfig.Mode == OpModeRender {
		renderDataFromConfig()
	}
	duration := time.Now().Sub(tStart)
	log.Printf("Took: %f sec", duration.Seconds())

	//	log.Fatal("Can't mix input/output types both must be either directories or files")

	//singleFileTest("/Users/gnilk/Downloads/tmpout/apple142.png", "output_segments.bin", "output_cont.png")
	//singleFileTest("apple142.png", "output_segments.bin", "")
	//multiFileTest("/Users/gnilk/Downloads/tmpout/", "segdir/")
	//readAndDrawSegmentFile("segdir/apple56.png.seg", "seg_cont.png")
	//readAndDrawMultiSeg(1, 3080, "segimages/")
}

func parseOptions() {
	if len(os.Args) > 1 {
		for i := 1; i < len(os.Args); i++ {
			arg := os.Args[i]
			if arg == "-lcd" {
				i++
				glbConfig.LineCutOffDistance, _ = strconv.ParseFloat(os.Args[i], 64)
			} else if arg == "-gl" {
				i++
				tmp, _ := strconv.Atoi(os.Args[i])
				glbConfig.GreyThresholdLevel = uint8(tmp)
			} else if arg == "-lca" {
				i++
				glbConfig.LineCutOffAngle, _ = strconv.ParseFloat(os.Args[i], 64)
			} else if arg == "-cnt" {
				i++
				glbConfig.ContrastFactor, _ = strconv.ParseFloat(os.Args[i], 64)
			} else if arg == "-cns" {
				i++
				glbConfig.ContrastScale, _ = strconv.ParseFloat(os.Args[i], 64)
			} else if arg == "-oca" {
				i++
				glbConfig.OptimizationCutOffAngle, _ = strconv.ParseFloat(os.Args[i], 64)
				glbConfig.Optimize = true
			} else if arg == "-lld" {
				i++
				glbConfig.LongLineDistance, _ = strconv.ParseFloat(os.Args[i], 64)
			} else if arg == "-ccd" {
				i++
				glbConfig.ClusterCutOffDistance, _ = strconv.ParseFloat(os.Args[i], 64)
			} else if arg == "-frames" {
				i++
				glbConfig.NumFrames, _ = strconv.Atoi(os.Args[i])
			} else if arg == "-bs" {
				i++
				glbConfig.BlockSize, _ = strconv.Atoi(os.Args[i])
			} else if arg == "-defaults" {
				printDefaultsAndExit()
			} else if arg == "-datamode" {
				i++
				mode := os.Args[i]
				if mode == "int8" {
					glbConfig.DataMode = DataModeInt8
				} else if mode == "int16" {
					glbConfig.DataMode = DataModeInt16
				} else {
					log.Fatal("Illegal datamode, int8 or int16 is legal")
				}
			} else if arg[0] == '-' {
				for j := 0; j < len(arg); j++ {
					switch arg[j] {
					case 'g':
						glbConfig.Mode = OpModeGenerate
						break
					case 'r':
						glbConfig.Mode = OpModeRender
						break
					case 'F':
						glbConfig.OptimizedRangeSearch = false
						break
					case 'm':
						glbConfig.GenerateIntermediateData = true
						break
					case 'o':
						glbConfig.Optimize = true
						break
					case 'v':
						glbConfig.Verbose = true
						break
					case 'l':
						glbConfig.ListVectors = true
						break
					case 'e':
						glbConfig.SaveEmptySegmentFile = true
						break
					case 'w':
						i++
						glbConfig.Width, _ = strconv.Atoi(os.Args[i])
						glbConfig.Rescale = true
						break
					case 'h':
						i++
						glbConfig.Height, _ = strconv.Atoi(os.Args[i])
						glbConfig.Rescale = true
						break
					case '?':
						printHelpAndExit()
						break
					}
				}
			} else {
				// Input or Output
				if glbConfig.Input == "" {
					glbConfig.Input = arg
				} else if glbConfig.Output == "" {
					glbConfig.Output = arg
				} else {
					log.Fatal("Unknown argument: ", arg)
				}
			}
		}
	}
}

func isDir(name string) bool {
	f, err := os.Open(name)
	if err != nil {
		log.Fatal("isDir failed: ", err)
	}
	defer f.Close()

	fi, err := f.Stat()
	if err != nil {
		log.Fatal("isDir failed: ", err)
	}
	return fi.IsDir()
}

func printHelpAndExit() {
	fmt.Println("Usage:")
	fmt.Printf("contour <options> input output\n")
	fmt.Println("Input and Output can either be files or directories but not cominbed")
	fmt.Println("PNG is only format supported for input images")
	fmt.Println("Options")
	fmt.Println("  g   Generate line strips from PNG (file/dir) and save to strip file (default)")
	fmt.Println("  r   Render image from strip file and save as PNG file/directory")
	fmt.Println("  v   Switch on extensive output (this also renders debug data in the image)")
	fmt.Println("  e   Save empty segment files, default: false\n")
	fmt.Printf("   w   Generate: Rescale to width. Render: Use this width for destination bitmap, default: %d\n", glbConfig.Width)
	fmt.Println("  h   Generate: Rescale to height, keep zero to respect aspect ration from with, default: %d\n", glbConfig.Height)
	fmt.Println("  F   Switch on full range search (slow) when looking for points, default: false/off\n")
	fmt.Println("  ?   This screen")
	fmt.Println("Options for generating")
	fmt.Printf("  gl   <int>    Grey threshold level for contour lines when scanning bitmap, default: %d\n", glbConfig.GreyThresholdLevel)
	fmt.Printf("  bs   <int>    BlockSize for localized search, default: %d\n", glbConfig.BlockSize)
	fmt.Printf("  cnt <float>   Image contrast factor, default: %f\n", glbConfig.ContrastFactor)
	fmt.Printf("  cns <float>   Image contrast scale, default: %f\n", glbConfig.ContrastScale)
	fmt.Printf("  lcd <float>   Line Cutoff Distance, break condition for new segment, default: %f\n", glbConfig.LineCutOffDistance)
	fmt.Printf("  lca <float>   Line Cutoff Angle, break condition for new segment, default: %f\n", glbConfig.LineCutOffAngle)
	fmt.Printf("  lld <float>   Long Line Distance when searching for reference vector, default: %f\n", glbConfig.LongLineDistance)
	fmt.Printf("  ccd <float>   Cluster Cutoff Distance, break condition for a new polygon, default: %f\n", glbConfig.ClusterCutOffDistance)
	fmt.Printf("  oca <float>   Optimization Cutoff Angle, break condition for line segment concatination, default: %f\n", glbConfig.OptimizationCutOffAngle)
	fmt.Printf("Other options:")
	fmt.Printf("  frames <int>   Cut off generation after X number of frames\n")
	//fmt.Printf("  datamode <int8/int16>  Specify bitsize in segment file (read/write), default is int16\n")
	os.Exit(0)
}

func printDefaultsAndExit() {
	fmt.Printf("[defaults]\n")
	fmt.Printf("gl=%d\n", glbConfig.GreyThresholdLevel)
	fmt.Printf("bs=%d\n", glbConfig.BlockSize)
	fmt.Printf("cnt=%f\n", glbConfig.ContrastFactor)
	fmt.Printf("cns=%f\n", glbConfig.ContrastScale)
	fmt.Printf("lcd=%f\n", glbConfig.LineCutOffDistance)
	fmt.Printf("lca=%f\n", glbConfig.LineCutOffAngle)
	fmt.Printf("lld=%f\n", glbConfig.LongLineDistance)
	fmt.Printf("ccd=%f\n", glbConfig.ClusterCutOffDistance)
	fmt.Printf("oca=%f\n", glbConfig.OptimizationCutOffAngle)
	os.Exit(0)
}

// Runs the contour algorithm and saves to a strip file
func generateDataFromConfig() {

	log.Println("Optimized Range Search: ", glbConfig.OptimizedRangeSearch)
	isInputDir := isDir(glbConfig.Input)
	if isInputDir {
		log.Printf("Multi Processing Mode: %s -> %s\n", glbConfig.Input, glbConfig.Output)
		if glbConfig.GenerateIntermediateData {
			log.Println("WARN: Intermediate files not available in multi processing mode")
			glbConfig.GenerateIntermediateData = false // Switch this off!!
		}
		multiFileTest(glbConfig.Input, glbConfig.Output)
	} else {
		log.Printf("Generate Single File: %s -> %s\n", glbConfig.Input, glbConfig.Output)
		outputContour := ""
		if glbConfig.GenerateIntermediateData {
			outputContour = fmt.Sprintf("%s.png", glbConfig.Output)
			log.Printf("Saving Contour Image to: %s\n", outputContour)
		}
		singleFileTest(glbConfig.Input, glbConfig.Output, outputContour)
	}
}

func renderDataFromConfig() {

	if (glbConfig.Width == 0) || (glbConfig.Height == 0) {
		log.Fatal("Render needs width (-w) and height (-h) for destination image!")
	}

	readAndDrawStripDB(glbConfig.Input, glbConfig.Output)

	// isInputDir := isDir(glbConfig.Input)
	// if isInputDir {
	// 	if !isDir(glbConfig.Output) {
	// 		log.Fatal("If input is directory output must be directory!")
	// 	}
	// 	//log.Fatal("Not yet implemented!")
	// 	readAndDrawMultiSeg(1, 500, glbConfig.Input, glbConfig.Output)
	// } else {
	// 	log.Printf("Render Single File, %s -> %s\n", glbConfig.Input, glbConfig.Output)
	// 	readAndDrawSegmentFile(glbConfig.Input, glbConfig.Output)
	// }
}

type FileSeg struct {
	Filename string
	Segments int
}

//
// multiFileTest will process a complete directory of pictures and save (to another directory) line segment files
//
func multiFileTest(inputDirectory, outputDirectory string) {

	dir, err := os.Open(inputDirectory)
	if err != nil {
		log.Fatal("multiFileTest open input directory failed: ", err)
	}
	defer dir.Close()
	files, err := dir.Readdir(0)
	if err != nil {
		log.Fatal("multiFileTest reading directory failed: ", err)
	}
	numFiles := 0

	segmentsPerFrame := make([]FileSeg, 0)

	allSegments := make([][]*LineSegment, len(files)+1)

	for i, fileinfo := range files {
		inputFileName := path.Join(inputDirectory, fileinfo.Name())
		log.Printf("----------------------------------------------\n")
		log.Printf("%d of %d, processing: %s\n", numFiles, len(files), inputFileName)
		numSeg, lineSegments := singleFileTest(inputFileName, "", "")

		// Store in one big fat array
		var index int
		fmt.Sscanf(fileinfo.Name(), "apple%d.png", &index)
		log.Printf("FileIndex: %d\n", index)
		allSegments[index] = lineSegments
		//

		fs := FileSeg{
			Filename: "", //outputFileName,		// deprecated!
			Segments: numSeg,
		}

		segmentsPerFrame = append(segmentsPerFrame, fs)
		numFiles = numFiles + 1

		if glbConfig.NumFrames > 0 && (i+1) >= glbConfig.NumFrames {
			break
		}

	}
	log.Printf("---------------------------------\n")
	log.Printf("Processing completed, %d files\n", numFiles)

	log.Printf("Dumping to one big datafile")
	stripsfile, err := os.Create(glbConfig.Output)
	if err != nil {
		log.Fatal("Created segment database failed: ", err)
	}
	defer stripsfile.Close()

	for _, segarray := range allSegments {
		strips := LineSegmentsToStrips(segarray)
		// Write number of strips
		if len(strips) > 255 {
			log.Fatal("Failed: Too many strips in segment!")
		}

		// Format:
		// A bunch of lines connected together is called a strip. They are continious
		// from one point to the other
		//
		// A bunch of strips builds a frame
		//
		// Parse strips unilt EOF is hit
		//
		// Format details:
		//
		//  Number of Strips	int8
		//  [strip]
		//		Number of points	int8
		//		[points]
		//			X				int8
		//          Y               int8
		//
		//  - REPEAT UNTIL EOF
		//

		binary.Write(stripsfile, binary.LittleEndian, uint8(len(strips)))
		WriteStrips(stripsfile, strips)
	}
}

//
// singleFileTest converts a single image to a segment file and a rendering of the segments
//
func singleFileTest(inputFile, segmentsFile, contourFile string) (int, []*LineSegment) {
	img := ReadPNGImage(inputFile)
	log.Printf("Size: %d x %d\n", img.Bounds().Dx(), img.Bounds().Dy())

	width := img.Bounds().Dx()
	height := img.Bounds().Dy()

	img = increaseImageContrast(img)

	if glbConfig.GenerateIntermediateData {
		contrastImageName := fmt.Sprintf("%s_pp_contrast.png", segmentsFile)
		log.Printf("Saving Contrast Image to: %s\n", contrastImageName)
		SaveImage(img, contrastImageName)
	}

	if glbConfig.Rescale {
		if glbConfig.Height == 0 {
			aspectRatio := float64(height) / float64(width)
			glbConfig.Height = int(float64(glbConfig.Width) * aspectRatio)
		} else if glbConfig.Width == 0 {
			aspectRatio := float64(width) / float64(height)
			glbConfig.Width = int(float64(glbConfig.Height) * aspectRatio)
		}
		log.Printf(" Rescale to: %dx%d\n", glbConfig.Width, glbConfig.Height)
	} else {
		glbConfig.Width = width
		glbConfig.Height = height
	}

	// Preprocess image here if necessary

	numSegments := 0

	blocks := NewBlockMapFromImage(img)
	contourCluster := blocks.ExtractContour()
	if glbConfig.GenerateIntermediateData {
		contourImageName := fmt.Sprintf("%s_pp_contour.png", segmentsFile)
		// Note: Points not rescaled yet - we operate on original resolution as far as possible
		tmpImage := contourCluster.ImageFromCluster(width, height)
		log.Printf("Saving Contour Image to: %s\n", contourImageName)
		SaveImage(tmpImage, contourImageName)
	}
	lineSegments := ExtractVectors(contourCluster)
	if lineSegments != nil {
		if glbConfig.ListVectors == true {
			DumpLineSegments(lineSegments)
		}
		if glbConfig.Optimize {
			lineSegments = OptimizeLineSegments(lineSegments)
			//lineSegments = OptimizeLineSegments(lineSegments)
		}
		if glbConfig.Rescale {
			// Pass in original width/height
			lineSegments = RescaleLineSegments(lineSegments, width, height)
		}
		if segmentsFile != "" {
			SaveSegmentsAsStrips(glbConfig.Output, lineSegments)
		}
	} else {
		// Empty frame!!
		log.Printf("File: %s contains no data!\n", inputFile)
	}

	if len(contourFile) > 0 {
		log.Printf("Saving contour image to %s\n", contourFile)

		contImg := image.NewRGBA(image.Rect(0, 0, glbConfig.Width, glbConfig.Height))
		DrawLineSegments(lineSegments, contImg)
		SaveImage(contImg, contourFile)
	}
	return numSegments, lineSegments
}

func SaveSegmentsAsStrips(file string, lineSegments []*LineSegment) {
	stripsfile, err := os.Create(file)
	if err != nil {
		log.Fatal("Created segment database failed: ", err)
	}
	defer stripsfile.Close()
	log.Printf("Converting %d segments to strips\n", len(lineSegments))
	strips := LineSegmentsToStrips(lineSegments)
	// Write number of strips
	if len(strips) > 255 {
		log.Fatal("Failed: Too many strips in segment!")
	}
	log.Printf("Writing strips to: %s\n", file)

	binary.Write(stripsfile, binary.LittleEndian, uint8(len(strips)))
	WriteStrips(stripsfile, strips)
}

func rescale(v uint8) uint8 {
	tmp := float64(v) / float64(255)
	tmp = math.Pow(tmp, glbConfig.ContrastFactor) * glbConfig.ContrastScale
	tmp = tmp * 255
	if tmp > 255 {
		tmp = 255
	}
	v = uint8(tmp)
	return v
}

func increaseImageContrast(img image.Image) image.Image {
	width := img.Bounds().Dx()
	height := img.Bounds().Dy()
	dst := image.NewRGBA(image.Rect(0, 0, width, height))

	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			//c := img.At(x, y).(color.RGBA)
			rawcol := img.At(x, y)
			r, g, b, a := rawcol.RGBA()

			c := color.RGBA{
				R: uint8(r >> 8),
				G: uint8(g >> 8),
				B: uint8(b >> 8),
				A: uint8(a >> 8),
			}

			c.R = rescale(c.R)
			c.G = rescale(c.G)
			c.B = rescale(c.B)
			dst.Set(x, y, c)
		}
	}
	return dst

}

//
// readAndDrawMultiSeg renders all segment files in a directory to images in another directory
//
func readAndDrawMultiSeg(idxStart, idxEnd int, inputdir, outputdir string) {
	// TODO: Enhance this to scan the directory build a list and process that list
	//       See: multiFileTest

	dir, err := os.Open(inputdir)
	if err != nil {
		log.Fatal("readAndDrawMultiSeg open input directory failed: ", err)
	}
	defer dir.Close()
	files, err := dir.Readdir(0)
	if err != nil {
		log.Fatal("readAndDrawMultiSeg reading directory failed: ", err)
	}
	numFiles := 0

	//	segmentsPerFrame := make([]FileSeg, 0)

	for _, fileinfo := range files {
		inputFileName := path.Join(inputdir, fileinfo.Name())
		outputFileName := fmt.Sprintf("%s.png", path.Join(outputdir, fileinfo.Name()))
		log.Printf("%d of %d, rendering: %s -> %s\n", numFiles, len(files), inputFileName, outputFileName)
		readAndDrawSegmentFile(inputFileName, outputFileName)
		numFiles++
	}

	log.Printf("Procssed %d files\n", numFiles)

	// for i := idxStart; i < idxEnd; i++ {
	// 	inputName := path.Join(inputdir, fmt.Sprintf("apple%d.png.seg", i))
	// 	outputName := path.Join(outputdir, fmt.Sprintf("seq_cont_%d.png", i))
	// 	log.Printf("Processing %s -> %s\n", inputName, outputName)
	// 	readAndDrawSegmentFile(inputName, outputName)
	// }
}

func readAndDrawStripDB(inputFile, outputDir string) {
	stripFile, err := os.Open(inputFile)
	if err != nil {
		log.Fatal("readAndDrawStripDB, open strip file failure: ", err)
	}

	defer stripFile.Close()

	frameCounter := 0

	var stripsForFrame uint8
	//stripFile.Read()
	for {
		err = binary.Read(stripFile, binary.LittleEndian, &stripsForFrame)
		if err == io.EOF {
			log.Printf("EOF Reached, processed: %d frames\n", frameCounter)
			break
		}
		frame := readStripsForFrame(stripFile, stripsForFrame)
		frameFileName := path.Join(outputDir, fmt.Sprintf("image_%d.png", frameCounter))
		log.Printf("Frame: %d, strips: %d, Image: %s\n", frameCounter, stripsForFrame, frameFileName)
		renderStripsToFile(frameFileName, frame)
		frameCounter++
	}
}

func readStripsForFrame(reader io.Reader, nStrips uint8) []Strip {
	strips := make([]Strip, 0)
	for i := uint8(0); i < nStrips; i++ {
		var pointsInStrip uint8
		err := binary.Read(reader, binary.LittleEndian, &pointsInStrip)
		if err != nil {
			log.Fatal("readStripForFrame, number of points in strip failed: ", err)
		}
		log.Printf("  Points in strip: %d\n", pointsInStrip)
		strip := readStrip(reader, pointsInStrip)
		strips = append(strips, strip)
	}
	return strips
}
func readStrip(reader io.Reader, nPoints uint8) Strip {
	strip := make(Strip, 0)
	for i := uint8(0); i < nPoints; i++ {
		var x, y uint8
		binary.Read(reader, binary.LittleEndian, &x)
		binary.Read(reader, binary.LittleEndian, &y)
		pt := image.Pt(int(x), int(y))
		strip = append(strip, pt)
	}
	return strip
}

func renderStripsToFile(fileName string, strips []Strip) {
	img := image.NewRGBA(image.Rect(0, 0, glbConfig.Width, glbConfig.Height))
	lineSegments := LineSegmentsFromStrips(strips)
	DrawLineSegments(lineSegments, img)
	SaveImage(img, fileName)
}

//
// readAndDrawSegmentFile reads on segment file and renders an image saved to a PNG
//
func readAndDrawSegmentFile(inputFile, outputFile string) {
	lineSegments := ReadSEGFile(inputFile)
	img := image.NewRGBA(image.Rect(0, 0, glbConfig.Width, glbConfig.Height))
	DrawLineSegments(lineSegments, img)
	SaveImage(img, outputFile)
}

func ReadSEGFile(filename string) []*LineSegment {
	input, err := os.Open(filename)
	if err != nil {
		if !os.IsNotExist(err) {
			log.Fatal("ReadSEGFile failed: ", err)
		}
		return nil
	}
	defer input.Close()

	lineSegments := make([]*LineSegment, 0)
	var data []byte

	// Allocate depending on input data mode!
	if glbConfig.DataMode == DataModeInt16 {
		data = make([]byte, 4*2)
	} else if glbConfig.DataMode == DataModeInt8 {
		data = make([]byte, 4)
	}
	for {
		_, err := input.Read(data)
		if err != nil {
			if err != io.EOF {
				log.Fatal("ReadSEGFile failed: ", err)
			}
			break
		}
		ls := NewLineSegmentFromBytes(data)
		lineSegments = append(lineSegments, &ls)
	}
	return lineSegments
}

func ReadPNGImage(filename string) image.Image {
	input, err := os.Open(filename)
	if err != nil {
		log.Fatal("ReadPNGImage failed: ", err)
	}
	defer input.Close()

	img, err := png.Decode(input)
	if err != nil {
		log.Fatal("ReadPNGImage, decoding failed: ", err)
	}
	return img
}

func SaveImage(img image.Image, filename string) {
	outputFile, err := os.Create(filename)
	if err != nil {
		log.Fatal(err)
	}
	defer outputFile.Close()

	png.Encode(outputFile, img)
}

func SaveLineSegments(lineSegments []*LineSegment, filename string) {
	outputFile, err := os.Create(filename)
	if err != nil {
		log.Fatal(err)
		defer outputFile.Close()
	}

	log.Printf("Saving %d lines segments\n", len(lineSegments))

	totBytes := WriteLineSegments(outputFile, lineSegments)
	log.Printf("Wrote %d bytes to %s\n", totBytes, filename)
}

func WriteLineSegments(writer io.Writer, lineSegments []*LineSegment) int {
	totBytes := 0
	for _, ls := range lineSegments {
		buf := ls.ToBytes()
		n, err := writer.Write(buf.Bytes())
		if err != nil {
			log.Fatal("Unable to save linesegment to file")
		}
		totBytes = totBytes + n
	}
	return totBytes
}

func WriteLineSegmentsByteChannel(writer io.Writer, ch int, lineSegments []*LineSegment) int {
	totBytes := 0
	channelData := make([]byte, 1)
	for _, ls := range lineSegments {
		buf := ls.ToBytes()
		channelData[0] = buf.Bytes()[ch]
		n, err := writer.Write(channelData)
		if err != nil {
			log.Fatal("Unable to save linesegment to file")
		}
		totBytes = totBytes + n
	}
	return totBytes
}

//
// Block map constructors
//
type Block struct {
	X       int
	Y       int
	img     image.Image
	visited bool
	points  []*ContourPoint
}

func NewBlockMap() BlockMap {
	return make(BlockMap)
}

func NewBlockMapFromImage(img image.Image) BlockMap {
	blocks := NewBlockMap()

	width := img.Bounds().Dx()
	height := img.Bounds().Dy()

	for x := 0; x < width/glbConfig.BlockSize; x++ {
		for y := 0; y < height/glbConfig.BlockSize; y++ {
			block := NewBlock(img, x*glbConfig.BlockSize, y*glbConfig.BlockSize)
			blocks[block.Hash()] = &block
			//blocks = append(blocks, block)
		}
	}

	return blocks
}

func NewBlock(img image.Image, x, y int) Block {
	block := Block{
		X:       x,
		Y:       y,
		img:     img,
		visited: false,
		points:  make([]*ContourPoint, 0),
	}
	return block
}

func (b *Block) hashFunc(x, y int) int {
	return (x + y*1024)
}

func (b *Block) Hash() int {
	return b.hashFunc(b.X, b.Y)
}

func (b *Block) Left() int {
	return b.hashFunc(b.X-glbConfig.BlockSize, b.Y)
}
func (b *Block) Right() int {
	return b.hashFunc(b.X+glbConfig.BlockSize, b.Y)
}

func (b *Block) Up() int {
	return b.hashFunc(b.X, b.Y-glbConfig.BlockSize)
}
func (b *Block) Down() int {
	return b.hashFunc(b.X, b.Y+glbConfig.BlockSize)
}

func (b *Block) IsVisited() bool {
	return b.visited
}

func (b *Block) Visit() {
	b.visited = true
}

func (b *Block) IsFilled() bool {
	filledPixels := b.NumFilledPixels()
	// Should be threshold
	if filledPixels > 32 {
		return true
	}
	return false
}

func (b *Block) NumFilledPixels() int {
	filledPixels := 0
	for y := 0; y < glbConfig.BlockSize; y++ {
		for x := 0; x < glbConfig.BlockSize; x++ {
			clr := color.GrayModel.Convert(b.img.At(b.X+x, b.Y+y)).(color.Gray)
			// Should be threshold
			if clr.Y > glbConfig.GreyThresholdLevel {
				filledPixels = filledPixels + 1
			}
		}
	}
	return filledPixels
}

//
// Scan creates the contour
//
func (b *Block) ReadGreyPixel(x, y int) color.Gray {
	c := color.GrayModel.Convert(b.img.At(x, y)).(color.Gray)

	// tmp := float64(c.Y) / 255.0
	// tmp = tmp * tmp //math.Pow(tmp, 4)
	// c.Y = uint8(255.0 * tmp)
	return c
}

func (b *Block) Scan(pnts []*ContourPoint) []*ContourPoint {

	filledPixels := b.NumFilledPixels()
	if filledPixels == 0 {
		return pnts
	}
	t := float64(glbConfig.GreyThresholdLevel)

	for y := 0; y < glbConfig.BlockSize+1; y++ {
		for x := 0; x < glbConfig.BlockSize+1; x++ {
			if !image.Pt(b.X+x, b.Y+y).In(b.img.Bounds()) {
				continue
			}
			if !image.Pt(b.X+x-1, b.Y+y).In(b.img.Bounds()) {
				continue
			}
			if !image.Pt(b.X+x, b.Y+y-1).In(b.img.Bounds()) {
				continue
			}

			c := b.ReadGreyPixel(b.X+x, b.Y+y)
			l := b.ReadGreyPixel(b.X+x-1, b.Y+y)
			u := b.ReadGreyPixel(b.X+x, b.Y+y-1)
			//c := color.GrayModel.Convert(b.img.At(b.X+x, b.Y+y)).(color.Gray)
			// l := color.GrayModel.Convert(b.img.At(b.X+x-1, b.Y+y)).(color.Gray)
			// u := color.GrayModel.Convert(b.img.At(b.X+x, b.Y+y-1)).(color.Gray)

			delta_x := math.Abs(float64(l.Y) - float64(c.Y))
			delta_y := math.Abs(float64(u.Y) - float64(c.Y))
			if (delta_x > t) || (delta_y > t) {
				//fmt.Printf("(%d,%d),C:%d, L:%d, U:%d, dx:%f, dy:%f\n", b.X+x, b.Y+y, c.Y, l.Y, u.Y, delta_x, delta_y)
				cpt := NewContourPoint(image.Pt(b.X+x, b.Y+y), b)
				b.points = append(b.points, &cpt) // Add same point to this block
				pnts = append(pnts, &cpt)
			}
		}
	}

	return pnts
}

func (blocks BlockMap) GetBlock(h int) *Block {
	b, ok := blocks[h]
	if !ok {
		return nil
	}
	return b
}

func (blocks BlockMap) Left(block *Block) *Block {
	return blocks.GetBlock(block.Left())
}
func (blocks BlockMap) Right(block *Block) *Block {
	return blocks.GetBlock(block.Right())
}
func (blocks BlockMap) Up(block *Block) *Block {
	return blocks.GetBlock(block.Up())
}
func (blocks BlockMap) Down(block *Block) *Block {
	return blocks.GetBlock(block.Down())
}

var scannedBlocks int = 0

func (blocks BlockMap) Scan(pnts []*ContourPoint, b *Block) []*ContourPoint {
	if b.IsVisited() {
		return pnts
	}
	b.Visit()
	pnts = b.Scan(pnts)

	scannedBlocks = scannedBlocks + 1

	left := blocks.Left(b)
	if (left != nil) && (!left.IsVisited()) {
		pnts = blocks.Scan(pnts, left)
	}
	up := blocks.Up(b)
	if (up != nil) && (!up.IsVisited()) {
		pnts = blocks.Scan(pnts, up)
	}
	right := blocks.Right(b)
	if (right != nil) && (!right.IsVisited()) {
		pnts = blocks.Scan(pnts, right)
	}
	down := blocks.Down(b)
	if (down != nil) && (!down.IsVisited()) {
		pnts = blocks.Scan(pnts, down)
	}
	return pnts
}

// Contour point - better word is probably ClusterPoint
type ContourPoint struct {
	PIndex int // Index to self
	pt     image.Point
	block  *Block
	used   bool
}

type ContourCluster struct {
	points   []*ContourPoint
	blockMap *BlockMap
}

type LineSegment struct {
	ptStart  image.Point
	ptEnd    image.Point
	idxStart int
	idxEnd   int
}

type PointDistance struct {
	Distance float64
	PIndex   int
}

// Contour points
func (cp *ContourPoint) Pt() image.Point {
	return cp.pt
}

func (cp *ContourPoint) IsUsed() bool {
	return cp.used
}

func (cp *ContourPoint) Use() {
	cp.used = true
}

func (cp *ContourPoint) ResetUsage() {
	cp.used = false
}

func (cp *ContourPoint) GetBlock() *Block {
	return cp.block
}

func (cp *ContourPoint) Distance(other *ContourPoint) float64 {
	return VecLen(cp.Pt(), other.Pt())
}

func VecLen(p, pother image.Point) float64 {
	dx := float64(pother.X) - float64(p.X)
	dy := float64(pother.Y) - float64(p.Y)
	return math.Sqrt(dx*dx + dy*dy)
}

type PointDistances []PointDistance
type ByDistance struct{ PointDistances }

// Sorting interface
func (s PointDistances) Len() int      { return len(s) }
func (s PointDistances) Swap(i, j int) { s[i], s[j] = s[j], s[i] }
func (s ByDistance) Less(i, j int) bool {
	return s.PointDistances[i].Distance < s.PointDistances[j].Distance
}

func (blocks BlockMap) ExtractContour() ContourCluster {
	points := make([]*ContourPoint, 0)
	for _, b := range blocks {
		if !b.IsVisited() {
			points = blocks.Scan(points, b)
		}
	}
	// Set index of point in array - we use this later on...
	for i, pt := range points {
		pt.PIndex = i
	}

	cluster := ContourCluster{
		points:   points,
		blockMap: &blocks,
	}

	fmt.Printf("ContourPoints: %d\n", len(points))

	return cluster
}

func NewContourPoint(pt image.Point, b *Block) ContourPoint {
	cp := ContourPoint{
		pt:    pt,
		used:  false,
		block: b,
	}
	return cp
}

// func ContourClusterFromPointArray(points []image.Point) ContourCluster {
// 	cps := ContourCluster{}
// 	for _, p := range points {
// 		cp := NewContourPoint(p)
// 		cps = append(cps, cp)
// 	}
// 	return cps
// }

func NewLineSegment(a, b image.Point) LineSegment {
	ls := LineSegment{
		ptStart:  a,
		ptEnd:    b,
		idxStart: -1,
		idxEnd:   -1,
	}
	return ls
}
func NewLineSegmentFromBytes(data []byte) LineSegment {
	buf := bytes.NewBuffer(data)

	var x1, y1, x2, y2 uint16
	if glbConfig.DataMode == DataModeInt16 {
		binary.Read(buf, binary.LittleEndian, &x1)
		binary.Read(buf, binary.LittleEndian, &y1)
		binary.Read(buf, binary.LittleEndian, &x2)
		binary.Read(buf, binary.LittleEndian, &y2)
	} else if glbConfig.DataMode == DataModeInt8 {
		var tx1, ty1, tx2, ty2 uint8
		binary.Read(buf, binary.LittleEndian, &tx1)
		binary.Read(buf, binary.LittleEndian, &ty1)
		binary.Read(buf, binary.LittleEndian, &tx2)
		binary.Read(buf, binary.LittleEndian, &ty2)

		x1 = uint16(tx1)
		y1 = uint16(ty1)
		x2 = uint16(tx2)
		y2 = uint16(ty2)

	} else {
		log.Fatal("Unsupported datamode: ", glbConfig.DataMode)
	}
	//log.Printf("(%d,%d) (%d,%d)\n", x1, y1, x2, y2)

	ls := LineSegment{
		ptStart:  image.Pt(int(x1), int(y1)),
		ptEnd:    image.Pt(int(x2), int(y2)),
		idxStart: -1,
		idxEnd:   -1,
	}
	return ls
}

//
// LineSegment functions
//
func (ls LineSegment) PtStart() image.Point { return ls.ptStart }
func (ls LineSegment) PtEnd() image.Point   { return ls.ptEnd }
func (ls LineSegment) IdxStart() int        { return ls.idxStart }
func (ls LineSegment) IdxEnd() int          { return ls.idxEnd }
func (ls LineSegment) AsVector() vector.Vec2D {
	return vector.NewVec2DFromPoints(ls.ptStart, ls.ptEnd)
}
func (ls LineSegment) Length() float64 {
	return VecLen(ls.PtEnd(), ls.PtStart())
}

func PointToBytes(pt image.Point) *bytes.Buffer {
	out := new(bytes.Buffer)
	if glbConfig.DataMode == DataModeInt16 {
		binary.Write(out, binary.LittleEndian, uint16(pt.X))
		binary.Write(out, binary.LittleEndian, uint16(pt.Y))
	} else if glbConfig.DataMode == DataModeInt8 {
		binary.Write(out, binary.LittleEndian, uint8(pt.X))
		binary.Write(out, binary.LittleEndian, uint8(pt.Y))
	} else {
		log.Fatal("Usupported data mode: ", glbConfig.DataMode)
	}
	return out
}

func (ls *LineSegment) ToBytes() *bytes.Buffer {
	out := new(bytes.Buffer)
	if glbConfig.DataMode == DataModeInt16 {
		binary.Write(out, binary.LittleEndian, uint16(ls.PtStart().X))
		binary.Write(out, binary.LittleEndian, uint16(ls.PtStart().Y))
		binary.Write(out, binary.LittleEndian, uint16(ls.PtEnd().X))
		binary.Write(out, binary.LittleEndian, uint16(ls.PtEnd().Y))
	} else if glbConfig.DataMode == DataModeInt8 {
		binary.Write(out, binary.LittleEndian, uint8(ls.PtStart().X))
		binary.Write(out, binary.LittleEndian, uint8(ls.PtStart().Y))
		binary.Write(out, binary.LittleEndian, uint8(ls.PtEnd().X))
		binary.Write(out, binary.LittleEndian, uint8(ls.PtEnd().Y))
	} else {
		log.Fatal("Usupported data mode: ", glbConfig.DataMode)
	}
	return out
}

//
// Cluster functions
//
func (cluster ContourCluster) GetBlockMap() *BlockMap { return cluster.blockMap }
func (cluster ContourCluster) Len() int               { return len(cluster.points) }
func (cluster ContourCluster) At(i int) *ContourPoint { return cluster.points[i] }
func (cluster ContourCluster) NewLineSegment(a, b int) *LineSegment {
	ls := LineSegment{
		ptStart:  cluster.At(a).Pt(),
		ptEnd:    cluster.At(b).Pt(),
		idxStart: a,
		idxEnd:   b,
	}
	return &ls
}
func (cluster *ContourCluster) NewVector(a, b int) *vector.Vec2D {
	v := vector.NewVec2DFromPoints(cluster.At(a).Pt(), cluster.At(b).Pt())
	return &v
}

func (b *Block) CalcPointDistance(cp *ContourPoint, distances []PointDistance) []PointDistance {
	for _, pt := range b.points {
		if pt.PIndex == cp.PIndex {
			continue
		}
		if pt.IsUsed() {
			continue
		}

		pDist := PointDistance{
			Distance: cp.Distance(pt),
			PIndex:   pt.PIndex,
		}
		distances = append(distances, pDist)
	}
	return distances
}

// Does a local search
func (cluster *ContourCluster) LocalSearchPointDistance(pidx int) []PointDistance {
	porigin := cluster.At(pidx)
	distances := make([]PointDistance, 0)

	blockOrigin := porigin.GetBlock()

	distances = blockOrigin.CalcPointDistance(porigin, distances)
	left := cluster.blockMap.Left(blockOrigin)
	if left != nil {
		distances = left.CalcPointDistance(porigin, distances)
	}
	right := cluster.blockMap.Right(blockOrigin)
	if right != nil {
		distances = right.CalcPointDistance(porigin, distances)
	}

	up := cluster.blockMap.Up(blockOrigin)
	if up != nil {
		distances = up.CalcPointDistance(porigin, distances)

		left = cluster.blockMap.Left(up)
		if left != nil {
			distances = left.CalcPointDistance(porigin, distances)
		}
		right := cluster.blockMap.Right(up)
		if right != nil {
			distances = right.CalcPointDistance(porigin, distances)
		}
	}

	down := cluster.blockMap.Down(blockOrigin)
	if down != nil {
		distances = down.CalcPointDistance(porigin, distances)
		left = cluster.blockMap.Left(down)
		if left != nil {
			distances = left.CalcPointDistance(porigin, distances)
		}
		right := cluster.blockMap.Right(down)
		if right != nil {
			distances = right.CalcPointDistance(porigin, distances)
		}
	}
	// cluster.blockMap.Left(down).CalcPointDistance(porigin, distances)
	// cluster.blockMap.Right(down).CalcPointDistance(porigin, distances)

	return distances
}

func (cluster *ContourCluster) FullSearchPointDistance(pidx int) []PointDistance {
	//	Full range search - no block optimization, all points still in cluster taken into account
	porigin := cluster.At(pidx)
	distances := make([]PointDistance, 0)
	for i := 0; i < cluster.Len(); i++ {
		if i == pidx {
			continue
		}

		if cluster.At(i).IsUsed() {
			continue
		}

		pdist := PointDistance{
			Distance: porigin.Distance(cluster.At(i)),
			PIndex:   cluster.At(i).PIndex,
		}
		distances = append(distances, pdist)
	}

	return distances
}

//
// CalcPointDistance calculates the distance from point indicated by pidx to all other unused points
//
func (cluster *ContourCluster) CalcPointDistance(pidx int) []PointDistance {

	if glbConfig.OptimizedRangeSearch {
		res := cluster.LocalSearchPointDistance(pidx)
		if len(res) > 4 {
			return res
		}
		if glbConfig.Verbose {
			log.Printf("CalcPointDistance, local search revealed too few points, switching on full search!")
		}
	}
	return cluster.FullSearchPointDistance(pidx)

}

// This is the actual tracing algorithm.
//
// The basic idea is to accumulate points in the contour and break if the contour makes a sharp turn
// Sharp turn is detected with dot-product.
// Initially the algorithm accumulates points to make a reference vector
// before it switches on the dot-product handling (longLineMode)
//
// Controls to tweak:
//   Distance before creating reference vector
//   DotProduct deviation
//   Distance before leaving in case no reference vector was found [this is a rare case]
//   Cluster border
//
//
// Improvements: As the thickness of the contour can be different we should calculate an 'average' point
// around the accumulation distance.
//
//
//
func (cluster *ContourCluster) NextSegment(idxStart int) *LineSegment {

	// Calculate distance from all points to this point and sort low to high
	// Note: The CalcPointDistance will discard any 'Used'/'Visisted' points in the cluster
	pds := cluster.CalcPointDistance(idxStart)
	sort.Sort(ByDistance{pds})

	// No points left, discard
	if len(pds) < 2 {
		if glbConfig.Verbose {
			log.Printf("Too few points in cluster left!")
		}
		return nil
	}
	if glbConfig.Verbose {
		log.Printf("NextSegment, idxStart: %d, number of pds: %d\n", idxStart, len(pds))
	}

	// DUMMY!
	// for i := 0; i < 20; i++ {
	// 	if i >= len(pds) {
	// 		break
	// 	}
	// 	log.Printf("%d: %d, %f\n", i, pds[i].PIndex, pds[i].Distance)
	// }
	// os.Exit(1)

	longLineMode := false
	var vPrev *vector.Vec2D = nil
	var dp float64 = 0.0
	idxPrevious := -1
	for i, pd := range pds {

		// Detect a 'jump', this marks a cluster within the cluster
		if (idxPrevious == -1) && (pd.Distance > glbConfig.ClusterCutOffDistance) {
			cluster.At(pd.PIndex).Use()
			if glbConfig.Verbose {
				log.Printf("New Cluster detected, restarting loop")
			}
			return cluster.NextSegment(pd.PIndex)
		}

		if longLineMode {
			// vCurrent := vector.NewVec2DFromPoints(cluster.At(idxStart).Pt(), cluster.At(pd.PIndex).Pt())
			vCurrent := cluster.NewVector(idxStart, pd.PIndex)
			vCurrent.Norm()
			dp = vPrev.Dot(vCurrent)
			if glbConfig.Verbose {
				log.Printf("pd.PIndex: %d, dist: %f, dp: %f\n", pd.PIndex, pd.Distance, dp)
			}
		}

		//
		// break conditions - when a new segment has been found
		//
		if (longLineMode) && (dp < glbConfig.LineCutOffAngle) {
			if glbConfig.Verbose {
				log.Printf("NewSegment, angleCutOff, iter: %d, idxPrevious: %d, dist: %f, dp: %f\n", i, idxPrevious, pd.Distance, dp)
			}
			return cluster.NewLineSegment(idxStart, idxPrevious)
		} else if (idxPrevious != -1) && (pd.Distance > glbConfig.LineCutOffDistance) {
			if glbConfig.Verbose {
				log.Printf("NewSegment, lineCutOff, iter: %d, idxPrevious: %d, dist: %f, dp: %f\n", i, idxPrevious, pd.Distance, dp)
			}
			return cluster.NewLineSegment(idxStart, idxPrevious)
		} else if pd.Distance > glbConfig.LongLineDistance {
			// We have consumed enough distance from start, let's create a reference vector
			// capture a directional vector and switch to 'longLineMode' (dot-product checking)

			// TODO: average points close to this one when constructing the vector
			longLineMode = true
			vPrev = cluster.NewVector(idxStart, pd.PIndex)
			vPrev.Norm()
		}
		// Tag this point as used and save the index as 'previous' if we break
		cluster.At(pd.PIndex).Use()
		idxPrevious = pd.PIndex
	}

	//log.Printf("all PDS processed!!\n")

	if idxPrevious == -1 {
		// Bad??
		log.Println("No previous points, to few points left in cluster!!!")
		return nil
	}
	if glbConfig.Verbose {
		log.Printf("New Segment, out of range, iter: %d, idxPrevious: %d, dp: %f\n", len(pds), idxPrevious, dp)
	}
	// Line extends out from the search range, this generally happen in optimized search
	return cluster.NewLineSegment(idxStart, idxPrevious)
}

// ExtractVectors is used to extract continues linesegments from a set of points
func ExtractVectors(cluster ContourCluster) []*LineSegment {

	// Take all points and wrap them up so we can work with them. This add a bit of meta data
	//cluster := ContourClusterFromPointArray(points)

	if glbConfig.Verbose {
		log.Printf("Cluster Size: %d\n", cluster.Len())
	}

	if cluster.Len() < 2 {
		log.Printf("Empty cluster, skipping...")
		return nil
	}

	lineSegments := make([]*LineSegment, 0)
	dbgPoints := 0
	idxStart := 0
	//	log.Printf("Extract Vectors, idxStart: %d\n", idxStart)

	// Loop over all points and create segments
	for i := 0; i < cluster.Len(); i++ {
		ls := cluster.NextSegment(idxStart)
		// nil => no more segments can be produced from the cluster, leave!
		if ls == nil {
			if glbConfig.Verbose {
				log.Printf("Next segment returned nil - leaving, processed: %d of %d\n", i, cluster.Len())
			}
			break
		}
		lineSegments = append(lineSegments, ls)
		idxStart = ls.IdxEnd()

		dbgPoints = dbgPoints + 1

	}
	//log.Printf("Extract Vectors, got %d line segments\n", dbgPoints)
	return lineSegments
}

//
// Testing and drawing functions below this point
//
func isPointEqual(a, b image.Point) bool {
	if (a.X == b.X) && (a.Y == b.Y) {
		return true
	}
	return false
}

// TODO: This probably needs more work and tuning
// Clusters are now considerd
func OptimizeLineSegments(lineSegments []*LineSegment) []*LineSegment {
	newlist := make([]*LineSegment, 0)

	//	log.Printf("")

	for i := 1; i < len(lineSegments); i++ {
		lsStart := lineSegments[i-1]
		lsEnd := lineSegments[i]

		// Not sure this will work
		if isPointEqual(lsStart.PtEnd(), lsEnd.PtStart()) {
			vStart := lsStart.AsVector()
			vEnd := lsEnd.AsVector()
			vStart.Norm()
			vEnd.Norm()
			dev := vStart.Dot(&vEnd)

			if glbConfig.Verbose {
				log.Printf("%d, (%d,%d):(%d,%d) -> (%d,%d):(%d:%d) - dev: %f\n", i,
					lsStart.PtStart().X, lsStart.PtStart().Y, lsStart.PtEnd().X, lsStart.PtEnd().Y,
					lsEnd.PtStart().X, lsEnd.PtStart().Y, lsEnd.PtEnd().X, lsEnd.PtEnd().Y,
					dev)
			}

			// line segments aligned?
			if dev > glbConfig.OptimizationCutOffAngle {
				// Walk along until deviation is too big
				if glbConfig.Verbose {
					log.Printf("  -> Opt\n")
				}
				lsPrev := lsStart
				for ; dev > glbConfig.OptimizationCutOffAngle; i++ {

					if i == len(lineSegments) {
						break
					}
					lsEnd = lineSegments[i]
					if glbConfig.Verbose {
						log.Printf("   %d, (%d,%d):(%d,%d) - dev: %f\n", i,
							lsPrev.PtEnd().X, lsPrev.PtEnd().Y, lsEnd.PtStart().X, lsEnd.PtStart().Y, dev)
						//log.Printf("   	    (%d,%d):(%d,%d)\n", lsPrev.PtEnd().X, lsPrev.PtEnd().Y, lsEnd.PtStart().X, lsEnd.PtStart().Y)
					}

					// cluster check, if this does not belong to the same cluster (discontinuation), break loop and stop line optimization
					if !isPointEqual(lsPrev.PtEnd(), lsEnd.PtStart()) {
						if glbConfig.Verbose {
							log.Printf("    Break Opt, new cluster detected\n")
						}
						break
					}

					lsPrev = lsEnd // Save this, GO has no while loop so this is a bit ugly
					vEnd := lsEnd.AsVector()
					vEnd.Norm()
					dev = vStart.Dot(&vEnd)
				}
				// New segment is always between lsStart.Start and lsPrev.End
				lsNew := NewLineSegment(lsStart.PtStart(), lsPrev.PtEnd())
				if lsNew.Length() < 2 {
					log.Printf("   OPT: WARNING SHORT LS DETECTED!!!!!\n")
				}

				if glbConfig.Verbose {
					log.Printf("  <- Opt, dev: %f, length: %f\n", dev, lsNew.Length())
				}

				if glbConfig.Verbose {
					log.Printf("N:%d (%d,%d):(%d,%d), length: %f\n", i, lsNew.PtStart().X, lsNew.PtStart().Y, lsNew.PtEnd().X, lsNew.PtEnd().Y, lsNew.Length())
				}

				newlist = append(newlist, &lsNew)
			} else {
				if lsStart.Length() < 2 {
					log.Printf("NO-OPT, Short linesegment: %f, skipping\n", lsStart.Length())
				} else {
					// Can't optimize this segment, just push it to the new list
					newlist = append(newlist, lsStart)
				}

			}

		}
	}

	log.Printf("Optimize (oca: %f), segments before: %d, after: %d\n", glbConfig.OptimizationCutOffAngle, len(lineSegments), len(newlist))
	return newlist
}

type Strip []image.Point

func LineSegmentsFromStrips(strips []Strip) []*LineSegment {
	lineSegments := make([]*LineSegment, 0)
	for _, strip := range strips {
		for i := 0; i < len(strip)-1; i++ {
			ls := NewLineSegment(strip[i], strip[i+1])
			lineSegments = append(lineSegments, &ls)
		}
	}
	return lineSegments
}
func LineSegmentsToStrips(lineSegments []*LineSegment) []Strip {
	strips := make([]Strip, 0)
	strip := make(Strip, 0)

	if len(lineSegments) == 0 {
		return strips
	}

	var lsPrev *LineSegment

	for i := 0; i < len(lineSegments)-1; i++ {
		ls := lineSegments[i]
		if len(strip) > 1 {
			dist := VecLen(strip[len(strip)-1], ls.PtStart())
			if dist < 2 {
				if glbConfig.Verbose {
					log.Printf("  Warning: Short line segment detected, skipping")
				}
			} else {
				strip = append(strip, ls.ptStart)
			}
		} else {
			strip = append(strip, ls.ptStart)
		}

		lsNext := lineSegments[i+1]

		if isPointEqual(ls.PtEnd(), lsNext.PtStart()) {
			// If we are exceeding max 8bit length create new strip and continue
			// Note: This will break polygons!!!!
			if len(strip) > (255 - 2) {
				if glbConfig.Verbose {
					log.Printf("Strip exceeding 255 items, splitting\n")
				}
				strip = append(strip, ls.PtEnd())
				strips = append(strips, strip)
				strip = make(Strip, 0)
			}
		} else {
			// Append ending point and push forward
			strip = append(strip, ls.PtEnd())
			// New strip
			if glbConfig.Verbose {
				log.Printf("Store strip with %d points\n", len(strip))
			}
			strips = append(strips, strip)
			strip = make(Strip, 0)
		}
		lsPrev = ls
	}
	if len(lineSegments) > 1 {
		ls := lineSegments[len(lineSegments)-1]
		if isPointEqual(lsPrev.PtEnd(), ls.PtStart()) {
			if glbConfig.Verbose {
				log.Printf("Last LS append to current")
			}
			strip = append(strip, ls.PtEnd())
		} else {
			if glbConfig.Verbose {
				log.Printf("Last LS require new strip!!!")
			}
			if len(strip) > 0 {
				strip = append(strip, lsPrev.PtEnd())
				strips = append(strips, strip)
				if glbConfig.Verbose {
					log.Printf("  Adding stray point to last strip")
					log.Printf("Point in strip: %d\n", len(strip))
				}
				// New strip
				strip = make(Strip, 0)
			}
			strip = append(strip, ls.PtStart())
			strip = append(strip, ls.PtEnd())
			if glbConfig.Verbose {
				log.Printf("Point in strip: %d\n", len(strip))
			}
		}
		// Append last strip to all
		strips = append(strips, strip)
	} else {
		if glbConfig.Verbose {
			log.Printf("Only one segment, creating special strip!\n")
		}
		ls := lineSegments[0]
		strip = append(strip, ls.PtStart())
		strip = append(strip, ls.PtEnd())
		// Append last strip to all
		strips = append(strips, strip)
	}

	totalPoints := 0
	for i, s := range strips {
		if len(s) < 2 {
			if glbConfig.Verbose {
				log.Fatal("Strip %d for image contains invalid number of points: %d\n", i, len(s))
			}
		}
		totalPoints = totalPoints + len(s)
	}
	log.Printf("Total points in frame: %d\n", totalPoints)

	// TODO: Solve last segment!!!
	return strips

}

func WriteStrips(file io.Writer, strips []Strip) {
	log.Printf("Strips: %d\n", len(strips))
	for i, strip := range strips {
		nPoints := len(strip)
		log.Printf("  %d, Points: %d\n", i, nPoints)
		if nPoints > 255 {
			log.Fatal("WriterStrips Failed: More (%d) than 255 points in one strip!!!!!", len(strip))
		}
		binary.Write(file, binary.LittleEndian, uint8(nPoints))
		for _, pt := range strip {
			out := PointToBytes(pt)
			file.Write(out.Bytes())
		}
	}
}

func RescaleLineSegments(lineSegments []*LineSegment, w, h int) []*LineSegment {
	newlist := make([]*LineSegment, 0)
	// Eiher height or width or both have to be set in order for rescale to happen

	xFactor := 1.0 / float64(w)
	yFactor := 1.0 / float64(h)

	for _, ls := range lineSegments {
		ptStart := ls.PtStart()
		xs := int(float64(glbConfig.Width) * xFactor * float64(ptStart.X))
		ys := int(float64(glbConfig.Height) * yFactor * float64(ptStart.Y))

		ptEnd := ls.PtEnd()
		xe := int(float64(glbConfig.Width) * xFactor * float64(ptEnd.X))
		ye := int(float64(glbConfig.Height) * yFactor * float64(ptEnd.Y))

		newStart := image.Pt(xs, ys)
		newEnd := image.Pt(xe, ye)

		lsNew := NewLineSegment(newStart, newEnd)
		newlist = append(newlist, &lsNew)
	}

	return newlist
}

func DumpLineSegments(lineSegments []*LineSegment) {
	var lsPrev *LineSegment = nil
	var dev float64 = 0

	//OptimizeLineSegments(lineSegments)

	idxPoly := 0
	for i, ls := range lineSegments {
		if lsPrev != nil {
			if !isPointEqual(lsPrev.PtEnd(), ls.PtStart()) {
				idxPoly++
			} else {
				vPrev := lsPrev.AsVector()
				vCurr := ls.AsVector()
				vPrev.Norm()
				vCurr.Norm()
				dev = vPrev.Dot(&vCurr)
			}
		}
		fmt.Printf("%d P:%d (%d,%d):(%d,%d)\n", i, idxPoly, ls.PtStart().X, ls.PtStart().Y, ls.PtEnd().X, ls.PtEnd().Y)
		if lsPrev != nil {
			if dev > 0.95 {
				fmt.Printf("  dev: %f\n", dev)
			}
		}
		lsPrev = ls
	}
}

func PutPixel(dst *image.RGBA, p image.Point, col color.RGBA) {
	xPos := p.X
	yPos := p.Y
	dst.SetRGBA(int(xPos), int(yPos), col)
	dst.SetRGBA(int(xPos)+1, int(yPos), col)
	dst.SetRGBA(int(xPos), int(yPos)+1, col)
	dst.SetRGBA(int(xPos)+1, int(yPos)+1, col)
}

func DrawLine(dst *image.RGBA, a, b image.Point, col color.RGBA) {
	vlen := VecLen(a, b)
	xStep := (float64(b.X) - float64(a.X)) / vlen
	yStep := (float64(b.Y) - float64(a.Y)) / vlen

	xPos := float64(a.X)
	yPos := float64(a.Y)

	for i := 0; i < int(vlen); i++ {

		dst.SetRGBA(int(xPos), int(yPos), col)
		dst.SetRGBA(int(xPos)+1, int(yPos), col)
		dst.SetRGBA(int(xPos), int(yPos)+1, col)
		dst.SetRGBA(int(xPos)+1, int(yPos)+1, col)

		xPos = xPos + xStep
		yPos = yPos + yStep
	}
}

func DrawTestVectors(dst *image.RGBA) {

	ptOrigin := image.Pt(128, 128)
	vRef := vector.NewVec2D(0.0, 1.0)

	r := 64.0

	for i := 0; i < 16; i++ {
		ang := float64(i) * (2 * math.Pi / 16.0)
		x := r * math.Cos(ang)
		y := r * math.Sin(ang)

		vec := vector.NewVec2D(x, y)
		vec.Norm()
		dp := vec.Dot(&vRef)

		rLevel := uint8(0)
		gLevel := uint8(0)
		if dp > 0 {
			gLevel = uint8(255.0 * dp)
			//log.Printf("%d: ang: %f, dp: %f\n", i, ang, dp)
		} else {
			rLevel = uint8(math.Abs(255.0 * dp))
		}

		ptVec := image.Pt(int(x+128), int(y+128))
		DrawLine(dst, ptOrigin, ptVec, color.RGBA{rLevel, gLevel, 0, 255})

	}

}

func DrawLineSegments(lineSegments []*LineSegment, dst *image.RGBA) {
	// DrawLine(dst, image.Pt(0, 0), image.Pt(512, 512), color.RGBA{0, 0, 255, 255})
	// DrawTestVectors(dst)

	col := color.RGBA{255, 255, 255, 255}
	ptcol := color.RGBA{255, 0, 0, 255}
	ptcol2 := color.RGBA{0, 0, 255, 255}

	for i := range lineSegments {
		ls := lineSegments[i]
		//DrawLine(dst, ls.PtStart(), ls.PtEnd(), color.RGBA{0, 0, uint8(i & 255), 255})
		DrawLine(dst, ls.PtStart(), ls.PtEnd(), col)
		if glbConfig.Verbose {
			if i == 0 {
				hl := image.Pt(ls.PtStart().X-4, ls.PtStart().Y)
				hr := image.Pt(ls.PtStart().X+4, ls.PtStart().Y)
				vu := image.Pt(ls.PtStart().X, ls.PtStart().Y-4)
				vd := image.Pt(ls.PtStart().X, ls.PtStart().Y+4)
				DrawLine(dst, hl, hr, ptcol2)
				DrawLine(dst, vu, vd, ptcol2)
				//PutPixel(dst, ls.PtStart(), ptcol2)
			} else {
				PutPixel(dst, ls.PtStart(), ptcol)
			}

		}
	}
}

func (cluster *ContourCluster) ImageFromCluster(w, h int) image.Image {

	//dst := image.NewGray(image.Rect(0, 0, w, h))
	dst := image.NewRGBA(image.Rect(0, 0, w, h))

	// fmt.Printf("Num Points: %d\n", len(points))
	// fmt.Printf("Scanned Blocks: %d\n", scannedBlocks)

	level := uint8(255)
	for i := 0; i < cluster.Len(); i++ {
		//level := uint8(i & 255)
		//dst.SetGray(p.X, p.Y, color.Gray{level})
		p := cluster.At(i).Pt()
		dst.SetRGBA(p.X, p.Y, color.RGBA{level, 0, 0, 255})
	}

	for _, b := range *cluster.blockMap {
		OutlineBlock(dst, b.X, b.Y, b.X+glbConfig.BlockSize, b.Y+glbConfig.BlockSize, color.RGBA{0, 0, level, 64})
	}

	return dst
}

func (blocks BlockMap) ImageFromFilledBlocks(w, h int) image.Image {
	dst := image.NewGray(image.Rect(0, 0, w, h))

	for _, b := range blocks {
		if b.IsFilled() {
			FillBlock(dst, b.X, b.Y, 255)
		}
	}

	return dst
}

func FillBlock(img *image.Gray, bx, by int, level uint8) {
	for y := 0; y < glbConfig.BlockSize; y++ {
		for x := 0; x < glbConfig.BlockSize; x++ {
			img.SetGray(bx+x, by+y, color.Gray{level})
		}
	}
}

func OutlineBlock(img *image.RGBA, x1, y1, x2, y2 int, col color.RGBA) {
	for x := x1; x < x2; x++ {
		img.SetRGBA(x, y1, col)
		img.SetRGBA(x, y2, col)
	}
	for y := y1; y < y2; y++ {
		img.SetRGBA(x1, y, col)
		img.SetRGBA(x2, y, col)
	}
}
