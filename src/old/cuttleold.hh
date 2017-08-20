#pragma once

#include <QMainWindow>
#include <QDir>
#include <QList>
#include <QVBoxLayout>
#include <QFrame>
#include <QProgressBar>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>

#include "../imgview.hh"

#include <vector>

struct CuttleSet;
class CuttleCore;

struct CuttleImg final {
	CuttleImg() = delete;
	CuttleImg(QFileInfo);
	CuttleImg(CuttleImg const &) = delete;
	CuttleImg(CuttleImg &&) = delete;
	~CuttleImg() = default;
	
	QFileInfo file;
	int res = 0;
	std::vector<QColor> data;
	CuttleSet * set = nullptr;
	
	QImage getImage();
	void resize(int res);
	static float compare(CuttleImg & A, CuttleImg & B, int res);
};

struct CuttleSet final {
	CuttleSet() = default;
	CuttleSet(CuttleSet const &) = delete;
	CuttleSet(CuttleSet &&) = delete;
	~CuttleSet() { for(CuttleImg * img : imgs) img->set = nullptr; }
	
	QList<CuttleImg *> imgs {};
	
	void add(CuttleImg * img) { imgs.append(img); img->set = this; }
	void merge(CuttleSet & other);
};

class CuttleImgDisplay : public QFrame {
	Q_OBJECT
signals:
	void viewImage(CuttleImgDisplay *);
public:
	CuttleImgDisplay(CuttleCore * core, CuttleImg * img);
	
	CuttleImg * img = nullptr;
private slots:
	void doView();
};

class CuttleCollection : public QFrame {
	Q_OBJECT
signals:
	void display(CuttleCollection *);
	void removeThis(CuttleCollection *);
public:
	CuttleCollection(CuttleCore *, CuttleSet *);
	virtual ~CuttleCollection();
	CuttleSet * set = nullptr;
	int updateCount();
private slots:
	void doDisplay();
	void doRemove();
private:
	QLabel * itemLabel = nullptr;
};

class CuttleCore : public QMainWindow {
	Q_OBJECT
public:
	CuttleCore();
	virtual ~CuttleCore() = default;
public slots:
	void showCollection(CuttleCollection *);
	void showImage(CuttleImgDisplay *);
	void removeCollection(CuttleCollection *);
protected slots:
	void begin(QString dir);
	void beginDialog();
	void recalculate();
	void setName(QString);
	void removeCurrent();
	void deleteCurrent();
private:
	QString lastDir = QString::null;
	int lastRes = -1;
	ImageView * view = nullptr;
	QVBoxLayout * collectionLayout = nullptr;
	QProgressBar * progress = nullptr;
	QSpinBox * resSpin = nullptr;
	QDoubleSpinBox * threshSpin = nullptr;
	QHBoxLayout * displayLayout = nullptr;
	QLineEdit * curName = nullptr;
	QList<CuttleImg *> imgs {};
	QList<CuttleSet *> sets {};
	QList<CuttleCollection *> collections {};
	QList<CuttleImgDisplay *> displays {};
	
	CuttleCollection * curCollection = nullptr;
	CuttleImgDisplay * curImg = nullptr;
};
