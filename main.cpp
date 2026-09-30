#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QMessageBox>

#include <QColor>
#include <QLabel>
#include <QtDebug>
#include <QString>
#include <QPushButton>

#include "LeptonThread.h"
#include "MyLabel.h"

void printUsage(char *cmd) {
        char *cmdname = basename(cmd);
	printf("Usage: %s [OPTION]...\n"
               " -h      display this help and exit\n"
               " -cm x   select colormap\n"
               "           1 : rainbow\n"
               "           2 : grayscale\n"
               "           3 : ironblack [default]\n"
               " -tl x   select type of Lepton\n"
               "           2 : Lepton 2.x [default]\n"
               "           3 : Lepton 3.x\n"
               "               [for your reference] Please use nice command\n"
               "                 e.g. sudo nice -n 0 ./%s -tl 3\n"
               " -ss x   SPI bus speed [MHz] (10 - 30)\n"
               "           20 : 20MHz [default]\n"
               " -min x  override minimum value for scaling (0 - 65535)\n"
               "           [default] automatic scaling range adjustment\n"
               "           e.g. -min 30000\n"
               " -max x  override maximum value for scaling (0 - 65535)\n"
               "           [default] automatic scaling range adjustment\n"
               "           e.g. -max 32000\n"
               " -d x    log level (0-255)\n"
               "", cmdname, cmdname);
	return;
}

int main( int argc, char **argv )
{
	int typeColormap = 3; // colormap_ironblack
	int typeLepton = 2; // Lepton 2.x
	int spiSpeed = 20; // SPI bus speed 20MHz
	int rangeMin = -1; //
	int rangeMax = -1; //
	int loglevel = 0;
	for(int i=1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			printUsage(argv[0]);
			exit(0);
		}
		else if (strcmp(argv[i], "-d") == 0) {
			int val = 3;
			if ((i + 1 != argc) && (strncmp(argv[i + 1], "-", 1) != 0)) {
				val = std::atoi(argv[i + 1]);
				i++;
			}
			if (0 <= val) {
				loglevel = val & 0xFF;
			}
		}
		else if ((strcmp(argv[i], "-cm") == 0) && (i + 1 != argc)) {
			int val = std::atoi(argv[i + 1]);
			if ((val == 1) || (val == 2)) {
				typeColormap = val;
				i++;
			}
		}
		else if ((strcmp(argv[i], "-tl") == 0) && (i + 1 != argc)) {
			int val = std::atoi(argv[i + 1]);
			if (val == 3) {
				typeLepton = val;
				i++;
			}
		}
		else if ((strcmp(argv[i], "-ss") == 0) && (i + 1 != argc)) {
			int val = std::atoi(argv[i + 1]);
			if ((10 <= val) && (val <= 30)) {
				spiSpeed = val;
				i++;
			}
		}
		else if ((strcmp(argv[i], "-min") == 0) && (i + 1 != argc)) {
			int val = std::atoi(argv[i + 1]);
			if ((0 <= val) && (val <= 65535)) {
				rangeMin = val;
				i++;
			}
		}
		else if ((strcmp(argv[i], "-max") == 0) && (i + 1 != argc)) {
			int val = std::atoi(argv[i + 1]);
			if ((0 <= val) && (val <= 65535)) {
				rangeMax = val;
				i++;
			}
		}
	}

	//create the app
	QApplication a( argc, argv );
	
	QWidget *myWidget = new QWidget;
	myWidget->setGeometry(400, 300, 540, 290);
	myWidget->setWindowTitle("Thermal Camera - Lepton 3.1R");

	//create an image placeholder for myLabel
	//fill the top left corner with red, just bcuz
	QImage myImage;
	myImage = QImage(320, 240, QImage::Format_RGB888);
	QRgb red = qRgb(255,0,0);
	for(int i=0;i<80;i++) {
		for(int j=0;j<60;j++) {
			myImage.setPixel(i, j, red);
		}
	}

	//create a label, and set it's image to the placeholder
	MyLabel myLabel(myWidget);
	myLabel.setGeometry(10, 10, 520, 240); 
	myLabel.setPixmap(QPixmap::fromImage(myImage));
	
	// Create temperature legend - NEW
	QLabel *legendLabel = new QLabel(myWidget);
	legendLabel->setGeometry(440, 10, 80, 240);  // Right side of thermal image
	legendLabel->setStyleSheet("background-color: black; border: 1px solid white;");
	legendLabel->setAlignment(Qt::AlignCenter);
	legendLabel->setText("Loading...");
	legendLabel->setWordWrap(true);

	
    //create a FFC button
	QPushButton *button1 = new QPushButton("Calibrate", myWidget);
	button1->setGeometry(10, 255, 100, 30);
	button1->setStyleSheet(
		"QPushButton { background-color: #4a4a4a; color: white; border: 2px solid #666; border-radius: 5px; font-weight: bold; }"
		"QPushButton:hover { background-color: #5a5a5a; }"
		"QPushButton:pressed { background-color: #2a2a2a; border: 2px solid white; }"
	);
	
	// Create Palette button
	QPushButton *paletteButton = new QPushButton("Palette: Iron", myWidget);
	paletteButton->setGeometry(120, 255, 100, 30);
	paletteButton->setStyleSheet(
		"QPushButton { background-color: #4a4a4a; color: white; border: 2px solid #666; border-radius: 5px; font-weight: bold; }"
		"QPushButton:hover { background-color: #5a5a5a; }"
		"QPushButton:pressed { background-color: #2a2a2a; border: 2px solid white; }"
	);
	
	// Overlay Toggle Button
	QPushButton *overlayButton = new QPushButton("T", myWidget);
	overlayButton->setGeometry(450, 255, 80, 30);
	overlayButton->setStyleSheet(
		"QPushButton { background-color: #4a4a4a; color: white; border: 2px solid #666; border-radius: 5px; font-weight: bold; }"
		"QPushButton:hover { background-color: #5a5a5a; }"
		"QPushButton:pressed { background-color: #2a2a2a; border: 2px solid white; }"
	);
	
	// Create Start Recording button
	QPushButton *recordButton = new QPushButton("● Record", myWidget);
	recordButton->setGeometry(230, 255, 100, 30);
	recordButton->setStyleSheet(
		"QPushButton { background-color: #cc0000; color: white; border: 2px solid #666; border-radius: 5px; font-weight: bold; }"
		"QPushButton:hover { background-color: #dd0000; }"
		"QPushButton:pressed { background-color: #880000; border: 2px solid white; }"
	);
	
	// Create Stop Recording button
	QPushButton *stopButton = new QPushButton("■ Stop", myWidget);
	stopButton->setGeometry(340, 255, 100, 30);
	stopButton->setStyleSheet(
		"QPushButton { background-color: #4a4a4a; color: white; border: 2px solid #666; border-radius: 5px; font-weight: bold; }"
		"QPushButton:hover { background-color: #5a5a5a; }"
		"QPushButton:pressed { background-color: #2a2a2a; border: 2px solid white; }"
	);
	
	// Create recording status indicator
	QLabel *statusLabel = new QLabel("", myWidget);
	statusLabel->setGeometry(10, 225, 410, 25);
	statusLabel->setAlignment(Qt::AlignCenter);
	statusLabel->setStyleSheet(
		"background-color: rgba(0,0,0,0); "
		"color: white; "
		"font-size: 14px; "
		"font-weight: bold;"
	);
	statusLabel->hide();  // Hidden by default
	

	//create a thread to gather SPI data
	//when the thread emits updateImage, the label should update its image accordingly
	LeptonThread *thread = new LeptonThread();
	thread->setLogLevel(loglevel);
	thread->useColormap(typeColormap);
	thread->useLepton(typeLepton);
	thread->useSpiSpeedMhz(spiSpeed);
	thread->setAutomaticScalingRange();
	if (0 <= rangeMin) thread->useRangeMinValue(rangeMin);
	if (0 <= rangeMax) thread->useRangeMaxValue(rangeMax);
	QObject::connect(thread, SIGNAL(updateImage(QImage)), &myLabel, SLOT(setImage(QImage)));
	
	QObject::connect(thread, SIGNAL(updateImage(QImage)), &myLabel, SLOT(setImage(QImage)));
	
	//connect ffc button to the thread's ffc action
	QObject::connect(button1, SIGNAL(clicked()), thread, SLOT(performFFC()));
	
	// Connect Start Recording button
	QObject::connect(recordButton, &QPushButton::clicked, [thread, statusLabel, recordButton]() {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        //QString filename = QString("thermal_%1.avi").arg(timestamp);
        thread->startRecording(timestamp);
        
        // Show recording status
		statusLabel->setText("🔴 RECORDING");
		statusLabel->setStyleSheet(
			"background-color: rgba(204,0,0,180); "
			"color: white; "
			"font-size: 14px; "
			"font-weight: bold; "
			"border-radius: 5px; "
			"padding: 5px;"
		);
		statusLabel->show();
		
		// Change button color when recording
		recordButton->setStyleSheet(
			"QPushButton { background-color: #880000; color: white; border: 2px solid white; border-radius: 5px; font-weight: bold; }"
			"QPushButton:hover { background-color: #990000; }"
			"QPushButton:pressed { background-color: #660000; border: 2px solid yellow; }"
		);
    });
    
	// Connect Stop Recording button
	QObject::connect(stopButton, &QPushButton::clicked, [thread, statusLabel, recordButton]() {
    thread->stopRecording();
    
    // Hide recording status
		statusLabel->hide();
		
		// Reset record button color
		recordButton->setStyleSheet(
			"QPushButton { background-color: #cc0000; color: white; border: 2px solid #666; border-radius: 5px; font-weight: bold; }"
			"QPushButton:hover { background-color: #dd0000; }"
			"QPushButton:pressed { background-color: #880000; border: 2px solid white; }"
		);
    
    });
    
    // Connect temperature updates to legend (only when overlay is OFF)
	QObject::connect(thread, &LeptonThread::updateTemperatures, 
		[legendLabel](float minF, float maxF, float avgF) {
			QString tempText = QString("MAX\n%1°F\n\nMID\n%2°F\n\nMIN\n%3°F")
				.arg(maxF, 0, 'f', 1)
				.arg((minF + maxF) / 2.0, 0, 'f', 1)
				.arg(minF, 0, 'f', 1);
			
			legendLabel->setText(tempText);
			legendLabel->setStyleSheet(
				"background-color: black; "
				"color: white; "
				"border: 1px solid white; "
				"padding: 10px; "
				"font-size: 12px; "
				"font-weight: bold;"
			);
		});
    
    // Connect Palette button
	QObject::connect(paletteButton, &QPushButton::clicked, 
		[thread, paletteButton]() {
			static int currentPalette = 3;  // Make static so it persists
			
			// Cycle through palettes: 3 (iron) → 1 (rainbow) → 2 (gray) → 3
			currentPalette++;
			if (currentPalette > 3) currentPalette = 1;
			
			thread->changeColormap(currentPalette);
			
			// Update button text
			QString paletteName;
			switch(currentPalette) {
				case 1: paletteName = "Rainbow"; break;
				case 2: paletteName = "Gray"; break;
				case 3: paletteName = "Iron"; break;
			}
			paletteButton->setText("Palette: " + paletteName);
		});
		
		// Connect Overlay Toggle
	QObject::connect(overlayButton, &QPushButton::clicked, 
		[thread, overlayButton, legendLabel, myWidget]() {
			static bool enabled = false;
			enabled = !enabled;
			
			thread->setOverlayTemps(enabled);
			
			if (enabled) {
				legendLabel->hide();
				myWidget->setGeometry(400, 300, 540, 290);  // Narrower window
				overlayButton->setText("T: V");
			} else {
				legendLabel->show();
				myWidget->setGeometry(400, 300, 540, 290);  // Original width
				overlayButton->setText("T: S");
			}
		});
    
	thread->start();
	
	myWidget->show();

	return a.exec();
}

