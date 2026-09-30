#ifndef TEXTTHREAD
#define TEXTTHREAD

#include <ctime>
#include <stdint.h>
#include <queue>

#include <QThread>
#include <QtCore>
#include <QPixmap>
#include <QImage>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <fstream> //csv file writing
#include <sys/stat.h> //directory creation
#include <QPainter>
#include <QFont>

#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 60
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)

class LeptonThread : public QThread
{
  Q_OBJECT;

public:
  LeptonThread();
  ~LeptonThread();

  void setLogLevel(uint16_t);
  void useColormap(int);
  void useLepton(int);
  void useSpiSpeedMhz(unsigned int);
  void setAutomaticScalingRange();
  void useRangeMinValue(uint16_t);
  void useRangeMaxValue(uint16_t);
  void run();

public slots:
  void performFFC();
  void startRecording(QString filename);
  void stopRecording();
  void changeColormap(int newColormap); 
  void setOverlayTemps(bool enabled);

signals:
  void updateText(QString);
  void updateImage(QImage);
  void updateTemperatures(float minF, float maxF, float avgF);

private:
  
  void writeFrameToVideo(const QImage &image);
  void log_message(uint16_t, std::string);
  void createRecordingDirectories();
  float kelvinToFahrenheit(float kelvin);
  uint16_t loglevel;
  int typeColormap;
  const int *selectedColormap;
  int selectedColormapSize;
  int typeLepton;
  unsigned int spiSpeed;
  bool autoRangeMin;
  bool autoRangeMax;
  uint16_t rangeMin;
  uint16_t rangeMax;
  int myImageWidth;
  int myImageHeight;
  QImage myImage;
  cv::VideoWriter videoWriter;
  bool isRecording;
  std::string videoFilename;
  std::queue<QImage> frameQueue;
  QMutex queueMutex;
  int droppedFrames;
  bool overlayTemps;
 
  
  //temp tracking
  float currentMinTempF;
  float currentMaxTempF;
  float currentAvgTempF;
  
  //csv recording
  std::ofstream csvFile;
  std::string csvFilename;
  int recordingFrameCount;

  uint8_t result[PACKET_SIZE*PACKETS_PER_FRAME];
  uint8_t shelf[4][PACKET_SIZE*PACKETS_PER_FRAME];
  uint16_t *frameBuffer;

};

#endif
