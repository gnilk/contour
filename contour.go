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
// 2) Detect outline based on Luma values -> ContourImage
// 3) Convert contourimage to a point cluster
// 4) Traverse the point cluster and create lines segments
//
//
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
//   ffmpeg -i video.avi -u music.mp3 -codec copy -shortest mixed.avi
//

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"io/ioutil"
	"log"
	"math"
	"os"
	"path"
	"sort"
	"strconv"

	vector "contour/vector"
)

type Block struct {
	X       int
	Y       int
	img     image.Image
	visited bool
}

type OpMode int64

const (
	_                     = iota
	OpModeGenerate OpMode = 10
	OpModeRender   OpMode = 20
)

type Config struct {
	GreyThresholdLevel      uint8
	ClusterCutOffDistance   float64
	LineCutOffDistance      float64
	LineCutOffAngle         float64
	LongLineDistance        float64
	OptimizationCutOffAngle float64
	BlockSize               int
	FilledBlockLevel        float32
	// Options
	Verbose                  bool
	Mode                     OpMode
	Input                    string
	Output                   string
	GenerateIntermediateData bool
	ListVectors              bool
	Optimize                 bool
}

var glbConfig = Config{
	GreyThresholdLevel:      32,   // This is the contour threshold value
	ClusterCutOffDistance:   50,   // This is when a point is considered to be a new cluster
	LineCutOffDistance:      10.0, // Creates new segment when distance is too far also not dependent on the distance vector in case of sparse clusters
	LineCutOffAngle:         0.5,  // Creates a new segment when deviating angle hits this threshold, only comes in play when we can derive a directional vector (see: LongLineDistance)
	LongLineDistance:        3.0,  // Distance to search before capturing the directional vector for the segment
	BlockSize:               64,   // Not used
	FilledBlockLevel:        0.5,  // Not used
	OptimizationCutOffAngle: 0.97,
	// Options
	Verbose: false, // Switch on heavy debug output
	Mode:    OpModeGenerate,
	Input:   "",
	Output:  "",
	GenerateIntermediateData: false,
	ListVectors:              false,
	Optimize:                 false,
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

	if glbConfig.Mode == OpModeGenerate {
		generateDataFromConfig()
	} else if glbConfig.Mode == OpModeRender {
		renderDataFromConfig()
	}

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
			} else if arg == "-lca" {
				i++
				glbConfig.LineCutOffAngle, _ = strconv.ParseFloat(os.Args[i], 64)
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
			} else if arg[0] == '-' {
				for j := 0; j < len(arg); j++ {
					switch arg[j] {
					case 'g':
						glbConfig.Mode = OpModeGenerate
						break
					case 'r':
						glbConfig.Mode = OpModeRender
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
					case '?':
					case 'h':
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
	fmt.Println("  g   Generate line segments from PNG save to Segment file/directory (default)")
	fmt.Println("  o   Enable optimization")
	fmt.Println("  r   Render image from segment file/directory and save as PNG file/directory")
	fmt.Println("  v   Switch on extensive output")
	fmt.Println("  ?/h This screen")
	fmt.Println("Options for generating")
	fmt.Printf("  lcd <float>   Line Cutoff Distance, break condition for new segment, default: %f\n", glbConfig.LineCutOffDistance)
	fmt.Printf("  lca <float>   Line Cutoff Angle, break condition for new segment, default: %f\n", glbConfig.LineCutOffAngle)
	fmt.Printf("  lld <float>   Long Line Distance when searching for reference vector, default: %f\n", glbConfig.LongLineDistance)
	fmt.Printf("  ccd <float>   Cluster Cutoff Distance, break condition for a new polygon, default: %f\n", glbConfig.ClusterCutOffDistance)
	fmt.Printf("  oca <float>   Optimization Cutoff Angle, break condition for line segment concatination, default: %f\n", glbConfig.OptimizationCutOffAngle)
	os.Exit(1)
}

func generateDataFromConfig() {

	isInputDir := isDir(glbConfig.Input)

	if isInputDir {
		if !isDir(glbConfig.Output) {
			log.Fatal("If input is directory output must be directory!")
		}
		log.Printf("Multi Processing Mode: %s -> %s\n", glbConfig.Input, glbConfig.Output)
		if glbConfig.GenerateIntermediateData {
			log.Println("WARN: Intermediate files not available in multi processing mode")
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
	isInputDir := isDir(glbConfig.Input)
	if isInputDir {
		if !isDir(glbConfig.Output) {
			log.Fatal("If input is directory output must be directory!")
		}
		log.Fatal("Not yet implemented!")
		//readAndDrawMultiSeg(1, 3080, "segimages/")
	} else {
		log.Printf("Render Single File, %s -> %s\n", glbConfig.Input, glbConfig.Output)
		readAndDrawSegmentFile(glbConfig.Input, glbConfig.Output)
	}
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

	for _, fileinfo := range files {
		inputFileName := path.Join(inputDirectory, fileinfo.Name())
		outputFileName := fmt.Sprintf("%s.seg", path.Join(outputDirectory, fileinfo.Name()))
		log.Printf("----------------------------------------------\n")
		log.Printf("Processing file: %s\n", inputFileName)
		numSeg := singleFileTest(inputFileName, outputFileName, "")

		fs := FileSeg{
			Filename: outputFileName,
			Segments: numSeg,
		}

		segmentsPerFrame = append(segmentsPerFrame, fs)
		numFiles = numFiles + 1

	}
	log.Printf("---------------------------------\n")
	log.Printf("Processing completed, %d files\n", numFiles)

	strSegments := ""
	for _, s := range segmentsPerFrame {
		strSegments += fmt.Sprintf("%s,%d\n", s.Filename, s.Segments)
	}
	byteCode := []byte(strSegments)
	ioutil.WriteFile("segments.txt", byteCode, 0644)
}

//
// singleFileTest converts a single image to a segment file and a rendering of the segments
//
func singleFileTest(inputFile, segmentsFile, contourFile string) int {
	img := ReadPNGImage(inputFile)
	log.Printf("Size: %d x %d\n", img.Bounds().Dx(), img.Bounds().Dy())

	width := img.Bounds().Dx()
	height := img.Bounds().Dy()

	numSegments := 0

	blocks := NewBlockMapFromImage(img)
	points := blocks.ExtractContour()
	lineSegments := ExtractVectors(points)
	if lineSegments != nil {

		if glbConfig.ListVectors == true {
			DumpLineSegments(lineSegments)
		}
		if glbConfig.Optimize {
			lineSegments = OptimizeLineSegments(lineSegments)
		}
		//lsOpt := OptimizeLineSegments(lineSegments)
		// if glbConfig.ListVectors == true {
		// 	DumpLineSegments(lsOpt)
		// }

		SaveLineSegments(lineSegments, segmentsFile)
		numSegments = len(lineSegments)
	}

	if len(contourFile) > 0 {
		log.Printf("Saving contour image to %s\n", contourFile)
		contImg := image.NewRGBA(image.Rect(0, 0, width, height))
		DrawLineSegments(lineSegments, contImg)
		SaveImage(contImg, contourFile)
	}
	return numSegments
}

//
// readAndDrawMultiSeg renders all segment files in a directory to images in another directory
//
func readAndDrawMultiSeg(idxStart, idxEnd int, outputdir string) {
	for i := idxStart; i < idxEnd; i++ {
		inputName := path.Join("segdir/", fmt.Sprintf("apple%d.png.seg", i))
		outputName := path.Join(outputdir, fmt.Sprintf("seq_cont_%d.png", i))
		log.Printf("%s\n", outputName)
		readAndDrawSegmentFile(inputName, outputName)
	}
}

//
// readAndDrawSegmentFile reads on segment file and renders an image saved to a PNG
//
func readAndDrawSegmentFile(inputFile, outputFile string) {
	lineSegments := ReadSEGFile(inputFile)
	img := image.NewRGBA(image.Rect(0, 0, 960, 720))
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
	data := make([]byte, 4*2) // each line segment is 4 points of 16 bits
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

	totBytes := 0
	for _, ls := range lineSegments {
		buf := ls.ToBytes()
		n, err := outputFile.Write(buf.Bytes())
		if err != nil {
			log.Fatal("Unable to save linesegment to file")
		}
		totBytes = totBytes + n
	}

	log.Printf("Wrote %d bytes to %s\n", totBytes, filename)

}

//
// Block map constructors
//
func NewBlockMap() BlockMap {
	return make(BlockMap)
}

func NewBlockMapFromImage(img image.Image) BlockMap {
	blocks := NewBlockMap()

	width := img.Bounds().Dx()
	height := img.Bounds().Dy()

	for x := 0; x < width/8; x++ {
		for y := 0; y < height/8; y++ {
			block := NewBlock(img, x*8, y*8)
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
	}
	return block
}

func (b *Block) hashFunc(x, y int) int {
	return (x + y*255)
}

func (b *Block) Hash() int {
	return b.hashFunc(b.X, b.Y)
}

func (b *Block) Left() int {
	return b.hashFunc(b.X-8, b.Y)
}
func (b *Block) Right() int {
	return b.hashFunc(b.X+8, b.Y)
}

func (b *Block) Up() int {
	return b.hashFunc(b.X, b.Y-8)
}
func (b *Block) Down() int {
	return b.hashFunc(b.X, b.Y+8)
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
	for y := 0; y < 8; y++ {
		for x := 0; x < 8; x++ {
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
func (b *Block) Scan(pnts []image.Point) []image.Point {

	filledPixels := b.NumFilledPixels()
	if filledPixels == 0 {
		return pnts
	}
	t := float64(glbConfig.GreyThresholdLevel)

	for y := 0; y < 9; y++ {
		for x := 0; x < 9; x++ {
			if !image.Pt(b.X+x, b.Y+y).In(b.img.Bounds()) {
				continue
			}
			if !image.Pt(b.X+x-1, b.Y+y).In(b.img.Bounds()) {
				continue
			}
			if !image.Pt(b.X+x, b.Y+y-1).In(b.img.Bounds()) {
				continue
			}

			c := color.GrayModel.Convert(b.img.At(b.X+x, b.Y+y)).(color.Gray)
			l := color.GrayModel.Convert(b.img.At(b.X+x-1, b.Y+y)).(color.Gray)
			u := color.GrayModel.Convert(b.img.At(b.X+x, b.Y+y-1)).(color.Gray)

			delta_x := math.Abs(float64(l.Y) - float64(c.Y))
			delta_y := math.Abs(float64(u.Y) - float64(c.Y))
			if (delta_x > t) || (delta_y > t) {
				//fmt.Printf("(%d,%d),C:%d, L:%d, U:%d, dx:%f, dy:%f\n", b.X+x, b.Y+y, c.Y, l.Y, u.Y, delta_x, delta_y)
				pnts = append(pnts, image.Pt(b.X+x, b.Y+y))
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

func (blocks BlockMap) Scan(pnts []image.Point, b *Block) []image.Point {
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
	pt   image.Point
	used bool
}

type ContourCluster []ContourPoint

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

func (blocks BlockMap) ExtractContour() []image.Point {
	points := make([]image.Point, 0)
	for _, b := range blocks {
		if !b.IsVisited() {
			points = blocks.Scan(points, b)
		}
	}
	return points
}

func NewContourPoint(pt image.Point) ContourPoint {
	cp := ContourPoint{
		pt:   pt,
		used: false,
	}
	return cp
}
func ContourClusterFromPointArray(points []image.Point) ContourCluster {
	cps := ContourCluster{}
	for _, p := range points {
		cp := NewContourPoint(p)
		cps = append(cps, cp)
	}
	return cps
}

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
	var x1, y1, x2, y2 int16
	binary.Read(buf, binary.LittleEndian, &x1)
	binary.Read(buf, binary.LittleEndian, &y1)
	binary.Read(buf, binary.LittleEndian, &x2)
	binary.Read(buf, binary.LittleEndian, &y2)

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

func (ls *LineSegment) ToBytes() *bytes.Buffer {
	out := new(bytes.Buffer)
	binary.Write(out, binary.LittleEndian, int16(ls.PtStart().X))
	binary.Write(out, binary.LittleEndian, int16(ls.PtStart().Y))
	binary.Write(out, binary.LittleEndian, int16(ls.PtEnd().X))
	binary.Write(out, binary.LittleEndian, int16(ls.PtEnd().Y))
	return out
}

//
// Cluster functions
//
func (cluster ContourCluster) Len() int               { return len(cluster) }
func (cluster ContourCluster) At(i int) *ContourPoint { return &cluster[i] }
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

//
// CalcPointDistance calculates the distance from point indicated by pidx to all other unused points
//
func (cluster *ContourCluster) CalcPointDistance(pidx int) []PointDistance {

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
			PIndex:   i,
		}
		distances = append(distances, pdist)
	}
	return distances
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
// Note: lsPrev is unused (deprecated)
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

	longLineMode := false
	var vPrev *vector.Vec2D = nil
	var dp float64 = 0.0
	idxPrevious := -1
	for i, pd := range pds {

		// Detect a 'jump', this marks a cluster within the cluster - should be configurable
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

	return nil
}

// ExtractVectors is used to extract continues linesegments from a set of points
func ExtractVectors(points []image.Point) []*LineSegment {

	// Take all points and wrap them up so we can work with them. This add a bit of meta data
	cluster := ContourClusterFromPointArray(points)

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
	log.Printf("Extract Vectors, idxStart: %d\n", idxStart)

	// Loop over all points and create segments
	for i := 0; i < cluster.Len(); i++ {
		ls := cluster.NextSegment(idxStart)
		// nil => no more segments can be produced from the cluster, leave!
		if ls == nil {
			if glbConfig.Verbose {
				log.Printf("Next segment returned nil - leaving")
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

// TODO: This needs more work, clusters should be considered!
func OptimizeLineSegments(lineSegments []*LineSegment) []*LineSegment {
	newlist := make([]*LineSegment, 0)

	//	log.Printf("")

	for i := 1; i < len(lineSegments); i++ {
		lsStart := lineSegments[i-1]
		lsEnd := lineSegments[i]

		if glbConfig.Verbose {
			log.Printf("%d (%d,%d):(%d,%d)\n", i, lsStart.PtStart().X, lsStart.PtStart().Y, lsStart.PtEnd().X, lsStart.PtEnd().Y)
		}

		// Not sure this will work
		if isPointEqual(lsStart.PtEnd(), lsEnd.PtStart()) {
			vStart := lsStart.AsVector()
			vEnd := lsEnd.AsVector()
			vStart.Norm()
			vEnd.Norm()
			dev := vStart.Dot(&vEnd)

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
						log.Printf("   %d, (%d,%d):(%d,%d) - dev: %f\n", i, lsPrev.PtEnd().X, lsPrev.PtEnd().Y, lsEnd.PtStart().X, lsEnd.PtStart().Y, dev)
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
				if glbConfig.Verbose {
					log.Printf("  <- Opt, dev: %f\n", dev)
				}
				// New segment is always between lsStart.Start and lsPrev.End
				lsNew := NewLineSegment(lsStart.PtStart(), lsPrev.PtEnd())
				if glbConfig.Verbose {
					log.Printf("%d (%d,%d):(%d,%d)\n", i, lsNew.PtStart().X, lsNew.PtStart().Y, lsNew.PtEnd().X, lsNew.PtEnd().Y)
				}

				newlist = append(newlist, &lsNew)
			} else {
				// Can't optimize this segment, just push it to the new list
				newlist = append(newlist, lsStart)
			}

		}
	}

	log.Printf("Optimize LS Before: %d, after: %d\n", len(lineSegments), len(newlist))
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

func (blocks BlockMap) ImageFromContour(points []image.Point, w, h int) image.Image {

	//dst := image.NewGray(image.Rect(0, 0, w, h))
	dst := image.NewRGBA(image.Rect(0, 0, w, h))

	fmt.Printf("Num Points: %d\n", len(points))
	fmt.Printf("Scanned Blocks: %d\n", scannedBlocks)

	// level := uint8(255)
	// for _, p := range points {
	// 	//level := uint8(i & 255)
	// 	//dst.SetGray(p.X, p.Y, color.Gray{level})
	// 	dst.SetRGBA(p.X, p.Y, color.RGBA{level, 0, 0, 255})
	// }
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
	for y := 0; y < 8; y++ {
		for x := 0; x < 8; x++ {
			img.SetGray(bx+x, by+y, color.Gray{level})
		}
	}
}
