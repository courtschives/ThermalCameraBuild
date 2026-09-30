#include <iostream>
#include <iomanip>  // For std::setprecision
#include "LeptonThread.h"

#include "Palettes.h"
#include "SPI.h"
#include "Lepton_I2C.h"
#include <QCoreApplication>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>

#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 60
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)
#define FPS 27;

LeptonThread::LeptonThread() : QThread()
{
	//
	loglevel = 0;

	//
	typeColormap = 3; // 1:colormap_rainbow  /  2:colormap_grayscale  /  3:colormap_ironblack(default)
	selectedColormap = colormap_ironblack;
	selectedColormapSize = get_size_colormap_ironblack();

	//
	typeLepton = 2; // 2:Lepton 2.x  / 3:Lepton 3.x
	myImageWidth = 80;
	myImageHeight = 60;

	//
	spiSpeed = 20 * 1000 * 1000; // SPI bus speed 20MHz

	// min/max value for scaling
	autoRangeMin = true;
	autoRangeMax = true;
	rangeMin = 30000;
	rangeMax = 32000;
	isRecording = false;
	videoFilename = "";
	droppedFrames = 0;
	
	//initialize temp tracking
	currentMinTempF = 0.0;
	currentMaxTempF = 0.0;
	currentAvgTempF = 0.0;
	recordingFrameCount = 0;
	overlayTemps = false;
}

LeptonThread::~LeptonThread() {
}

void LeptonThread::setLogLevel(uint16_t newLoglevel)
{
	loglevel = newLoglevel;
}

void LeptonThread::useColormap(int newTypeColormap)
{
	switch (newTypeColormap) {
	case 1:
		typeColormap = 1;
		selectedColormap = colormap_rainbow;
		selectedColormapSize = get_size_colormap_rainbow();
		break;
	case 2:
		typeColormap = 2;
		selectedColormap = colormap_grayscale;
		selectedColormapSize = get_size_colormap_grayscale();
		break;
	default:
		typeColormap = 3;
		selectedColormap = colormap_ironblack;
		selectedColormapSize = get_size_colormap_ironblack();
		break;
	}
}

void LeptonThread::useLepton(int newTypeLepton)
{
	switch (newTypeLepton) {
	case 3:
		typeLepton = 3;
		myImageWidth = 160;
		myImageHeight = 120;
		break;
	default:
		typeLepton = 2;
		myImageWidth = 80;
		myImageHeight = 60;
	}
}

void LeptonThread::useSpiSpeedMhz(unsigned int newSpiSpeed)
{
	spiSpeed = newSpiSpeed * 1000 * 1000;
}

void LeptonThread::setAutomaticScalingRange()
{
	autoRangeMin = true;
	autoRangeMax = true;
}

void LeptonThread::useRangeMinValue(uint16_t newMinValue)
{
	autoRangeMin = false;
	rangeMin = newMinValue;
}

void LeptonThread::useRangeMaxValue(uint16_t newMaxValue)
{
	autoRangeMax = false;
	rangeMax = newMaxValue;
}

void LeptonThread::run()
{
	//create the initial image
	myImage = QImage(myImageWidth, myImageHeight, QImage::Format_RGB888);

	const int *colormap;  // Will be updated each frame
	int colormapSize;     // Will be updated each frame
	uint16_t minValue = rangeMin;
	uint16_t maxValue = rangeMax;
	float diff = maxValue - minValue;
	float scale = 255/diff;
	uint16_t n_wrong_segment = 0;
	uint16_t n_zero_value_drop_frame = 0;

	//open spi port
	SpiOpenPort(0, spiSpeed);
	
	// Enable radiometric temperature mode 
	lepton_enable_radiometry();
	log_message(1, "Radiometric mode enabled");

	while(true) {
	
        // Update colormap pointer in case it changed
		colormap = selectedColormap;
		colormapSize = selectedColormapSize;

		//read data packets from lepton over SPI
		int resets = 0;
		int segmentNumber = -1;
		for(int j=0;j<PACKETS_PER_FRAME;j++) {
			//if it's a drop packet, reset j to 0, set to -1 so he'll be at 0 again loop
			read(spi_cs0_fd, result+sizeof(uint8_t)*PACKET_SIZE*j, sizeof(uint8_t)*PACKET_SIZE);
			int packetNumber = result[j*PACKET_SIZE+1];
			if(packetNumber != j) {
				j = -1;
				resets += 1;
				usleep(1000);
				//Note: we've selected 750 resets as an arbitrary limit, since there should never be 750 "null" packets between two valid transmissions at the current poll rate
				//By polling faster, developers may easily exceed this count, and the down period between frames may then be flagged as a loss of sync
				if(resets == 750) {
					SpiClosePort(0);
					lepton_reboot();
					n_wrong_segment = 0;
					n_zero_value_drop_frame = 0;
					usleep(750000);
					SpiOpenPort(0, spiSpeed);
				}
				continue;
			}
			if ((typeLepton == 3) && (packetNumber == 20)) {
				segmentNumber = (result[j*PACKET_SIZE] >> 4) & 0x0f;
				if ((segmentNumber < 1) || (4 < segmentNumber)) {
					log_message(10, "[ERROR] Wrong segment number " + std::to_string(segmentNumber));
					break;
				}
			}
		}
		if(resets >= 30) {
			log_message(3, "done reading, resets: " + std::to_string(resets));
		}


		//
		int iSegmentStart = 1;
		int iSegmentStop;
		if (typeLepton == 3) {
			if ((segmentNumber < 1) || (4 < segmentNumber)) {
				n_wrong_segment++;
				if ((n_wrong_segment % 12) == 0) {
					log_message(5, "[WARNING] Got wrong segment number continuously " + std::to_string(n_wrong_segment) + " times");
				}
				continue;
			}
			if (n_wrong_segment != 0) {
				log_message(8, "[WARNING] Got wrong segment number continuously " + std::to_string(n_wrong_segment) + " times [RECOVERED] : " + std::to_string(segmentNumber));
				n_wrong_segment = 0;
			}

			//
			memcpy(shelf[segmentNumber - 1], result, sizeof(uint8_t) * PACKET_SIZE*PACKETS_PER_FRAME);
			if (segmentNumber != 4) {
				continue;
			}
			iSegmentStop = 4;
		}
		else {
			memcpy(shelf[0], result, sizeof(uint8_t) * PACKET_SIZE*PACKETS_PER_FRAME);
			iSegmentStop = 1;
		}

		if ((autoRangeMin == true) || (autoRangeMax == true)) {
			if (autoRangeMin == true) {
				maxValue = 65535;
			}
			if (autoRangeMax == true) {
				maxValue = 0;
			}
			for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {
				for(int i=0;i<FRAME_SIZE_UINT16;i++) {
					//skip the first 2 uint16_t's of every packet, they're 4 header bytes
					if(i % PACKET_SIZE_UINT16 < 2) {
						continue;
					}

					//flip the MSB and LSB at the last second
					uint16_t value = (shelf[iSegment - 1][i*2] << 8) + shelf[iSegment - 1][i*2+1];
					if (value == 0) {
						// Why this value is 0?
						continue;
					}
					if ((autoRangeMax == true) && (value > maxValue)) {
						maxValue = value;
					}
					if ((autoRangeMin == true) && (value < minValue)) {
						minValue = value;
					}
				}
			}
			diff = maxValue - minValue;
			scale = 255/diff;
		}

		int row, column;
		uint16_t value;
		uint16_t valueFrameBuffer;
		QRgb color;
		
		// Calculate temps for this frame - MOVED OUTSIDE LOOP
		float minTempK = 999999.0;
		float maxTempK = 0.0;
		float sumTempK = 0.0;
		int tempPixelCount = 0;
		
		// First pass: calculate temperatures
		for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {
			for(int i=0;i<FRAME_SIZE_UINT16;i++) {
				if(i % PACKET_SIZE_UINT16 < 2) continue;
				
				uint16_t rawValue = (shelf[iSegment - 1][i*2] << 8) + shelf[iSegment - 1][i*2+1];
				if (rawValue == 0) continue;
				
				float tempK = 273.15 + (rawValue - 27315) / 100.0;
				
				if (tempK < minTempK) minTempK = tempK;
				if (tempK > maxTempK) maxTempK = tempK;
				sumTempK += tempK;
				tempPixelCount++;
			}
		}
		
		// Update temperature values once per frame
		if (tempPixelCount > 0) {
			currentMinTempF = kelvinToFahrenheit(minTempK);
			currentMaxTempF = kelvinToFahrenheit(maxTempK);
			currentAvgTempF = kelvinToFahrenheit(sumTempK / tempPixelCount);
		}
		
		// Second pass: draw pixels with colors
		for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {
			int ofsRow = 30 * (iSegment - 1);
			for(int i=0;i<FRAME_SIZE_UINT16;i++) {
				if(i % PACKET_SIZE_UINT16 < 2) continue;

				valueFrameBuffer = (shelf[iSegment - 1][i*2] << 8) + shelf[iSegment - 1][i*2+1];
				if (valueFrameBuffer == 0) {
					n_zero_value_drop_frame++;
					if ((n_zero_value_drop_frame % 12) == 0) {
						log_message(5, "[WARNING] Found zero-value. Drop the frame continuously " + std::to_string(n_zero_value_drop_frame) + " times");
					}
					break;
				}

				value = (valueFrameBuffer - minValue) * scale;
				int ofs_r = 3 * value + 0; if (colormapSize <= ofs_r) ofs_r = colormapSize - 1;
				int ofs_g = 3 * value + 1; if (colormapSize <= ofs_g) ofs_g = colormapSize - 1;
				int ofs_b = 3 * value + 2; if (colormapSize <= ofs_b) ofs_b = colormapSize - 1;
				color = qRgb(colormap[ofs_r], colormap[ofs_g], colormap[ofs_b]);
				
				if (typeLepton == 3) {
					column = (i % PACKET_SIZE_UINT16) - 2 + (myImageWidth / 2) * ((i % (PACKET_SIZE_UINT16 * 2)) / PACKET_SIZE_UINT16);
					row = i / PACKET_SIZE_UINT16 / 2 + ofsRow;
				}
				else {
					column = (i % PACKET_SIZE_UINT16) - 2;
					row = i / PACKET_SIZE_UINT16;
				}
				myImage.setPixel(column, row, color);
			}
		}

		if (n_zero_value_drop_frame != 0) {
			log_message(8, "[WARNING] Found zero-value. Drop the frame continuously " + std::to_string(n_zero_value_drop_frame) + " times [RECOVERED]");
			n_zero_value_drop_frame = 0;
		}
        
        
		// If recording, add frame to queue instead of writing directly
		if (isRecording) {
			writeFrameToVideo(myImage);
		}

		// Throttle display updates to reduce GUI load
		static int displayCounter = 0;
		displayCounter++;
		
		// Update display every 2nd frame (reduce by half)
		if (displayCounter % 2 == 0) {
		
               
            // Draw overlay if enabled
            if (overlayTemps) {
                QPainter painter(&myImage);
                QString tempStr = QString("MAX:%1°F  AVG:%2°F  MIN:%3°F")
                    .arg(currentMaxTempF, 0, 'f', 1)
                    .arg(currentAvgTempF, 0, 'f', 1)
                    .arg(currentMinTempF, 0, 'f', 1);
                painter.setFont(QFont("Arial", 6, QFont::Bold));
                QFontMetrics fm(painter.font());
                int textW = fm.horizontalAdvance(tempStr);
                int textH = fm.height();
                int x = myImageWidth - textW - 3;
                int y = myImageHeight - textH - 2;
                painter.fillRect(x - 1, y - 1, textW + 2, textH + 2, QColor(0, 0, 0, 160));
                painter.setPen(Qt::white);
                painter.drawText(x, y + fm.ascent(), tempStr);
            }

            // NOW record — after overlay has been painted onto myImage
            if (isRecording) {
                writeFrameToVideo(myImage);
            }

            // Emit to viewfinder
            emit updateImage(myImage);

            // Only send to legend if overlay is OFF
            if (!overlayTemps) {
                emit updateTemperatures(currentMinTempF, currentMaxTempF, currentAvgTempF);
            }
		}
		
		// Write to CSV if recording
		if (isRecording && csvFile.is_open()) {
			// Calculate timestamp in milliseconds
			int timestampMs = recordingFrameCount * 111;  // ~9fps = 111ms per frame
			
			csvFile << videoFilename << ","
					<< recordingFrameCount << ","
					<< timestampMs << ","
					<< std::fixed << std::setprecision(1)
					<< currentMinTempF << ","
					<< currentMaxTempF << ","
					<< currentAvgTempF << "\n";
			
			recordingFrameCount++;
		}
		
		// Process queued frames if recording
		if (isRecording) {
			// Process up to 3 queued frames per capture cycle
			for (int i = 0; i < 3; i++) {
				queueMutex.lock();
				if (frameQueue.empty()) {
					queueMutex.unlock();
					break;
				}
				QImage frameToWrite = frameQueue.front();
				frameQueue.pop();
				queueMutex.unlock();
				
				// Convert and write
				QImage rgbImage = frameToWrite.convertToFormat(QImage::Format_RGB888);
				cv::Mat opencvFrame(
					rgbImage.height(),
					rgbImage.width(),
					CV_8UC3,
					rgbImage.bits(),
					rgbImage.bytesPerLine()
				);
				
				cv::Mat bgrFrame;
				cv::cvtColor(opencvFrame, bgrFrame, cv::COLOR_RGB2BGR);
				videoWriter.write(bgrFrame);
			}
		}
		
		// Check overlay state from launcher
		static int overlayCheckCounter = 0;
		overlayCheckCounter++;
		if (overlayCheckCounter % 30 == 0) {  // Check every 30 frames (~3 seconds)
			std::ifstream overlayFile("/home/leovenus/thermal_overlay_state.txt");
			if (overlayFile.is_open()) {
				char state;
				overlayFile >> state;
				overlayTemps = (state == '1');
				overlayFile.close();
			}
		}
		
		// Process Qt events to keep GUI responsive
		QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
		
		// Small delay to prevent overwhelming the system
		//usleep(1000); // 1 millisecond
		
		
	}
	
	//finally, close SPI port just bcuz
	SpiClosePort(0);
}

void LeptonThread::performFFC() {
	//perform FFC
	lepton_perform_ffc();
}

void LeptonThread::log_message(uint16_t level, std::string msg)
{
	if (level <= loglevel) {
		std::cerr << msg << std::endl;
	}
}

void LeptonThread::startRecording(QString filename)
{
	// Create directories if they don't exist
	createRecordingDirectories();
	
	// Generate base filename (just the timestamp part)
	std::string baseFilename = filename.toStdString();
	
	// Create full paths
	videoFilename = "recordings/videos/" + baseFilename + ".avi";
	csvFilename = "recordings/data/" + baseFilename + ".csv";
	
	// Open video file
	//int fourcc = cv::VideoWriter::fourcc('M','J','P','G');
	int fourcc = 0;  // Uncompressed AVI - much faster, larger files
	double fps = 9.0;
	cv::Size frameSize(myImageWidth, myImageHeight);
	
	bool opened = videoWriter.open(videoFilename, fourcc, fps, frameSize, true);
	
	if(opened){
		// Open CSV file
		csvFile.open(csvFilename);
		if (csvFile.is_open()) {
			// Write CSV header
			csvFile << "video_id,frame,timestamp_ms,min_temp_f,max_temp_f,avg_temp_f\n";
			
			isRecording = true;
			droppedFrames = 0;
			recordingFrameCount = 0;
			
			// Clear frame queue
			queueMutex.lock();
			while (!frameQueue.empty()) {
				frameQueue.pop();
			}
			queueMutex.unlock();
			
			log_message(1, "Recording started: " + videoFilename);
			log_message(1, "CSV data: " + csvFilename);
		} else {
			videoWriter.release();
			log_message(1, "ERROR: Failed to create CSV file!");
		}
	} else {
		isRecording = false;
		log_message(1, "ERROR: Failed to start recording!");
	}
}

void LeptonThread::stopRecording()
{
	if (isRecording) {
		isRecording = false;
		
		// Write all remaining frames in queue
		log_message(1, "Flushing remaining frames...");
		queueMutex.lock();
		int remainingFrames = frameQueue.size();
		queueMutex.unlock();
		
		while (true) {
			queueMutex.lock();
			if (frameQueue.empty()) {
				queueMutex.unlock();
				break;
			}
			QImage frameToWrite = frameQueue.front();
			frameQueue.pop();
			queueMutex.unlock();
			
			// Convert and write
			QImage rgbImage = frameToWrite.convertToFormat(QImage::Format_RGB888);
			cv::Mat opencvFrame(
				rgbImage.height(),
				rgbImage.width(),
				CV_8UC3,
				rgbImage.bits(),
				rgbImage.bytesPerLine()
			);
			
			cv::Mat bgrFrame;
			cv::cvtColor(opencvFrame, bgrFrame, cv::COLOR_RGB2BGR);
			videoWriter.write(bgrFrame);
		}
		
		if (videoWriter.isOpened()) {
			videoWriter.release();
		}
		
		// Close CSV file 
		if (csvFile.is_open()) {
			csvFile.close();
		}
		
		log_message(1, "Recording stopped: " + videoFilename);
		log_message(1, "Flushed " + std::to_string(remainingFrames) + " remaining frames");
		if (droppedFrames > 0) {
			log_message(1, "WARNING: Dropped " + std::to_string(droppedFrames) + " frames during recording");
		}
	}
}
void LeptonThread::writeFrameToVideo(const QImage &image)
{
	// Add frame to queue with limit
	queueMutex.lock();
	const int MAX_QUEUE_SIZE = 90; // ~10 seconds at 9fps
	if (frameQueue.size() < MAX_QUEUE_SIZE) {
		frameQueue.push(image.copy());
	} else {
		droppedFrames++;
		if (droppedFrames % 10 == 0) {
			log_message(1, "WARNING: Frame queue full, dropped " + std::to_string(droppedFrames) + " frames");
		}
	}
	queueMutex.unlock();
}

void LeptonThread::createRecordingDirectories() {
	// Create recordings directory
	mkdir("recordings", 0755);
	// Create subdirectories
	mkdir("recordings/videos", 0755);
	mkdir("recordings/data", 0755);
}

float LeptonThread::kelvinToFahrenheit(float kelvin) {
	float celsius = kelvin - 273.15;
	float fahrenheit = celsius * 9.0 / 5.0 + 32.0;
	return fahrenheit;
}

void LeptonThread::changeColormap(int newColormap) {
	useColormap(newColormap);
	log_message(1, "Colormap changed to: " + std::to_string(newColormap));
}

void LeptonThread::setOverlayTemps(bool enabled) {
	overlayTemps = enabled;
}