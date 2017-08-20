#ifndef IMGVIEW_HPP
#define IMGVIEW_HPP

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QTimer>

#include <thread>

#include "rwsl.hh"

class ImageView : public QWidget {
	Q_OBJECT
public:
	enum ZKEEP {KEEP_NONE, KEEP_FIT, KEEP_FIT_FORCE, KEEP_EXPANDED, KEEP_EQUAL};
	ImageView(QWidget *parent = 0, QImage image = QImage(0, 0));
	virtual ~ImageView();
	QSize sizeHint() const;
	void paintEvent(QPaintEvent*);
	void resizeEvent(QResizeEvent *);
	void wheelEvent(QWheelEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	bool event(QEvent *);
	QImage getImage() {return view;}
	QImage getImageOfView();
	ZKEEP getKeepState() {return keep;}
public slots:
	void setImage(QImage, ZKEEP keepStart = KEEP_FIT);
	void setKeepState(ZKEEP z) {keep = z; this->update();}
protected slots:
	void centerView();
private slots:
	void hideMouse() {this->setCursor(Qt::BlankCursor);}
	void showMouse() {this->setCursor(Qt::ArrowCursor); mouseHider->start(500);}
private: //Variables
	QImage view;
	float zoom = 1.0f;
	QRect partRect;
	QRect drawRect;
	static float constexpr zoomMin = 0.025f;
	float zoomMax = 0.0f;
	float zoomExp = 0.0f;
	float zoomFit = 0.0f;
	ZKEEP keep;
	QPointF viewOffset = QPointF(0, 0);
	QPoint prevMPos;
	bool mouseMoving = false;
	bool paintCompletePartial = false;
	QPointF focalPoint;
	QTimer *mouseHider = new QTimer(this);
	bool touchOverride = false;
private: //Methods
	void setZoom(qreal, QPointF focus = QPointF(0, 0));
	void calculateZoomLevels();
	void calculateView();
private: //Bilinear
	std::atomic_bool bilGood {true};
	std::thread * bilWorker = nullptr;
	rwslck viewLock;
	rwslck drawLock;
	QImage bilCompare;
	QRect bilPart;
	QRect bilDraw;
	QImage bilRaster;
	void bilRun();
signals:
	void bilComplete();
	void bilProc();
};

#endif // IMGVIEW_HPP
