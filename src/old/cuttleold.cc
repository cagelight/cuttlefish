#include "cuttleold.hh"

#include <asterales/strop.hh>
#include "../rwsl.hh"

#include <atomic>
#include <thread>
#include <vector>

#include <QtWidgets>
#include <QImage>
#include <QImageReader>
#include <QDebug>

CuttleImg::CuttleImg(QFileInfo fi) : file(fi) {
	if (!file.exists()) {
		std::string fp { file.canonicalFilePath().toStdString() };
		throw std::runtime_error {asterales::vas("%s does not exist!", fp.c_str())};
	}
}

QImage CuttleImg::getImage() {
	QImageReader read {file.canonicalFilePath()};
	read.setAutoDetectImageFormat(true);
	read.setDecideFormatFromContent(true);
	return read.read();
}

void CuttleImg::resize(int res) {
	
	if (this->res == res) return;
	this->res = res;
	
	QImage img = getImage();
	if (img.isNull()) {
		std::string fp { file.canonicalFilePath().toStdString() };
		throw std::runtime_error {asterales::vas("image from (%s) is null!", fp.c_str())};
	}
	img = img.scaled({res, res}, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	data.resize(res * res);
	
	int i = 0;
	for (int y = 0; y < res; y++) {
		for (int x = 0; x < res; x++) {
			data[i++] = img.pixelColor(x, y);
		}
	}
}

float CuttleImg::compare(CuttleImg & A, CuttleImg & B, int res) {
	if (A.res != res) A.resize(res);
	if (B.res != res) B.resize(res);
	
	float match = 0;
	for (int i = 0; i < res * res; i++) {
		
		QColor & cA = A.data[i];
		QColor & cB = B.data[i];
		
		float mR = fabs(cA.redF() - cB.redF());
		float mG = fabs(cA.greenF() - cB.greenF());
		float mB = fabs(cA.blueF() - cB.blueF());
		float mL = mR;
		if (mL > mG) mL = mG;
		if (mL > mB) mL = mB;
		
		match += 1.0 - mL;
	}
	
	return match / (res * res);
}

void CuttleSet::merge(CuttleSet & other) {
	for (CuttleImg * img : other.imgs) {
		this->imgs.append(img);
		img->set = this;
	}
	other.imgs.clear();
}

static QString humanReadableSize(qint64 bytes) {
	if (bytes < 2048) return QString("%1 B").arg(bytes);
	else if (bytes < 2097152) return QString("%1 KiB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 2);
	else return QString("%1 MiB").arg(static_cast<double>(bytes) / 1048576.0, 0, 'f', 2);
}

CuttleImgDisplay::CuttleImgDisplay(CuttleCore * core, CuttleImg * img) : QFrame(core), img(img) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
	
	QGridLayout * layout = new QGridLayout {this};
	layout->setMargin(0);
	QPushButton * thumb = new QPushButton {this};
	
	QImage imgimg = img->getImage();
	
	QPixmap thumbpix = QPixmap::fromImage(imgimg.scaled(80, 80, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	thumb->setIcon(thumbpix);
	thumb->setIconSize(thumbpix.size());
	layout->addWidget(thumb, 0, 0, 1, 1);
	
	QLabel * formatLabel = new QLabel {humanReadableSize(img->file.size()) + " " + img->file.suffix().toUpper(), this};
	layout->addWidget(formatLabel, 1, 0, 1, 1);
	
	QLabel * sizeLabel = new QLabel {asterales::vas("%ix%i", imgimg.width(), imgimg.height()), this};
	layout->addWidget(sizeLabel, 2, 0, 1, 1);
	
	connect(thumb, SIGNAL(pressed()), this, SLOT(doView()));
	connect(this, SIGNAL(viewImage(CuttleImgDisplay *)), core, SLOT(showImage(CuttleImgDisplay *)));
}

void CuttleImgDisplay::doView() {
	emit viewImage(this);
}

CuttleCollection::CuttleCollection(CuttleCore * core, CuttleSet * set) : QFrame(core), set(set) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
	
	QGridLayout * layout = new QGridLayout(this);
	
	QLabel * preview = new QLabel {this};
	preview->setPixmap(QPixmap::fromImage(set->imgs.first()->getImage().scaled({50, 50}, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
	layout->addWidget(preview, 0, 0, 2, 1);
	
	itemLabel = new QLabel(asterales::vas("%i items", set->imgs.count()), this);
	layout->addWidget(itemLabel, 0, 1, 1, 2);
	
	QPushButton * goBut = new QPushButton {"Open", this};
	goBut->setMinimumWidth(20);
	layout->addWidget(goBut, 1, 1, 1, 1);
	
	QPushButton * remBut = new QPushButton {"Remove", this};
	layout->addWidget(remBut, 1, 2, 1, 1);
	
	connect(goBut, SIGNAL(pressed()), this, SLOT(doDisplay()));
	connect(remBut, SIGNAL(pressed()), this, SLOT(doRemove()));
	connect(this, SIGNAL(display(CuttleCollection *)), core, SLOT(showCollection(CuttleCollection *)));
	connect(this, SIGNAL(removeThis(CuttleCollection *)), core, SLOT(removeCollection(CuttleCollection *)));
}

CuttleCollection::~CuttleCollection() {
	
}

int CuttleCollection::updateCount() {
	itemLabel->setText(asterales::vas("%i items", set->imgs.count()));
	return set->imgs.count();
}

void CuttleCollection::doDisplay() {
	emit display(this);
}

void CuttleCollection::doRemove() {
	emit removeThis(this);
}

CuttleCore::CuttleCore() : QMainWindow() {
	
	QWidget * mainCont = new QWidget {this};
	this->setCentralWidget(mainCont);
	
	QGridLayout * mainLayout = new QGridLayout {mainCont};
	
	QScrollArea * leftCont = new QScrollArea {mainCont};
	leftCont->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	leftCont->setMinimumSize(300, 100);
	leftCont->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	leftCont->setWidgetResizable(true);
	QWidget * collectionBox = new QWidget {leftCont};
	leftCont->setWidget(collectionBox);
	collectionLayout = new QVBoxLayout {collectionBox};
	
	QWidget * rightCont = new QWidget {mainCont};
	QVBoxLayout * rightLayout = new QVBoxLayout {rightCont};
	rightLayout->setMargin(0);
	rightCont->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	
	QWidget * beginCont = new QWidget(mainCont);
	QHBoxLayout * beginLayout = new QHBoxLayout {beginCont};
	beginLayout->setMargin(0);
	
	progress = new QProgressBar {beginCont};
	resSpin = new QSpinBox {beginCont};
	resSpin->setMinimum(0);
	resSpin->setMaximum(65535);
	resSpin->setValue(32);
	threshSpin = new QDoubleSpinBox {beginCont};
	threshSpin->setMinimum(0.0001);
	threshSpin->setMaximum(0.9999);
	threshSpin->setSingleStep(0.0001);
	threshSpin->setDecimals(4);
	threshSpin->setValue(0.985);
	QPushButton * beginBut = new QPushButton {"Begin", beginCont};
	connect(beginBut, SIGNAL(pressed()), this, SLOT(beginDialog()));
	QPushButton * recalcBut = new QPushButton {"Recalc", beginCont};
	connect(recalcBut, SIGNAL(pressed()), this, SLOT(recalculate()));
	
	beginLayout->addWidget(new QLabel("Res:", beginCont));
	beginLayout->addWidget(resSpin);
	beginLayout->addWidget(new QLabel("Thresh:", beginCont));
	beginLayout->addWidget(threshSpin);
	beginLayout->addWidget(progress);
	beginLayout->addWidget(recalcBut);
	beginLayout->addWidget(beginBut);
	
	mainLayout->addWidget(leftCont, 0, 0, 1, 1);
	mainLayout->addWidget(rightCont, 0, 1, 1, 1);
	mainLayout->addWidget(beginCont, 1, 0, 1, 2);
	
	QScrollArea * curSetScroll = new QScrollArea {rightCont};
	curSetScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	curSetScroll->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	curSetScroll->setMinimumSize(500, 140);
	curSetScroll->setWidgetResizable(true);
	QWidget * displayCont = new QWidget {curSetScroll};
	displayCont->setMaximumHeight(120);
	curSetScroll->setWidget(displayCont);
	displayLayout = new QHBoxLayout {displayCont};
	rightLayout->addWidget(curSetScroll);
	
	view = new ImageView {rightCont};
	view->setMinimumSize(500, 500);
	rightLayout->addWidget(view);
	
	QWidget * curWidget = new QWidget { rightCont };
	QHBoxLayout * curLayout = new QHBoxLayout { curWidget };
	curLayout->setMargin(0);
	rightLayout->addWidget(curWidget);
	
	QPushButton * curRemove = new QPushButton { "Remove", rightCont };
	curLayout->addWidget(curRemove);
	connect(curRemove, SIGNAL(clicked()), this, SLOT(removeCurrent()));
	
	QPushButton * curDelete = new QPushButton { "Delete", rightCont };
	curLayout->addWidget(curDelete);
	connect(curDelete, SIGNAL(clicked()), this, SLOT(deleteCurrent()));
	
	curName = new QLineEdit { rightCont };
	curName->setReadOnly(true);
	curLayout->addWidget(curName);
}

void CuttleCore::showCollection(CuttleCollection * col) {
	for (CuttleImgDisplay * disp : displays) {
		delete disp;
	}
	displays.clear();
	
	curImg = nullptr;
	view->setImage({});
	
	if (!col || !col->set->imgs.size()) {
		curCollection = nullptr;
		return;
	}
	
	progress->setFormat("GENERATING THUMBNAILS: %p%");
	progress->setValue(0);
	progress->setMinimum(0);
	progress->setMaximum(col->set->imgs.count());
	int ccount = 0;
	for (CuttleImg * img : col->set->imgs) {
		CuttleImgDisplay * disp = new CuttleImgDisplay {this, img};
		displayLayout->addWidget(disp);
		displays.append(disp);
		progress->setValue(++ccount);
	}
	progress->setValue(col->set->imgs.count());
	progress->setFormat("DONE");
	
	curCollection = col;
}

void CuttleCore::showImage(CuttleImgDisplay * img) {
	view->setImage(img->img->getImage());
	setName(img->img->file.canonicalFilePath());
	
	Q_ASSERT(curCollection->set->imgs.contains(img->img));
	curImg = img;
}

void CuttleCore::removeCollection(CuttleCollection * col) {
	if (!col) return;
	delete col;
	collections.removeAll(col);
	if (curCollection == col) showCollection(nullptr);
} 

// ================================================================
// ----------------------------------------------------------------
// ================================================================

static rwslck beglk {};

static QFileInfoList load_files;
struct sim_check { int a, b; };
static std::vector<sim_check> sim_checks;
static std::atomic_uint_fast64_t begcur, begmax;

static void load_work(QProgressBar * progress, QList<CuttleImg *> * imgs, int res) {
	std::atomic_uint_fast64_t cur;
	while ((cur = begcur.fetch_add(1)) < begmax) {
		QFileInfo fi {load_files[cur]};
		beglk.write_lock();
		progress->setFormat("LOADING: " + fi.fileName());
		progress->setValue(cur);
		beglk.write_unlock();
		try {
			CuttleImg * img = new CuttleImg {fi};
			img->resize(res);
			beglk.write_lock();
			imgs->append(img);
			beglk.write_unlock();
		} catch (std::exception & e) {
			std::printf("ERROR: %s", e.what());
		}
	}
}

static void sim_work(QProgressBar * progress, QList<CuttleImg *> * imgs, QList<CuttleSet *> * sets, int res, float thresh) {
	std::atomic_uint_fast64_t cur;
	sim_check curchk;
	while ((cur = begcur.fetch_add(1)) < begmax) {
		
		beglk.write_lock();
		progress->setValue(cur);
		
		curchk = sim_checks[cur];
		
		if (curchk.a == curchk.b) {
			beglk.write_unlock();
			continue;
		}
		
		CuttleImg * ac = imgs->at(curchk.a);
		CuttleImg * bc = imgs->at(curchk.b);
		
		if (ac->set != nullptr && ac->set == bc->set) {
			beglk.write_unlock();
			continue;
		}
		
		beglk.write_unlock();
		
		float cmp = CuttleImg::compare(*ac, *bc, res);
		if (cmp < thresh) continue;
		
		beglk.write_lock();
		if (ac->set && bc->set && ac->set != bc->set) {
			CuttleSet * delset = bc->set;
			ac->set->merge(*delset);
			sets->removeAll(delset);
			delete delset;
		} else if (ac->set) {
			ac->set->add(bc);
		} else if (bc->set) {
			bc->set->add(ac);
		} else {
			CuttleSet * s = new CuttleSet {};
			s->add(ac);
			s->add(bc);
			sets->append(s);
		}
		beglk.write_unlock();
	}
}

// ----------------------------------------------------------------

void CuttleCore::begin(QString dirstr) {
	QDir dir {dirstr};
	if (!dir.exists()) return;
	
	curImg = nullptr;
	curCollection = nullptr;
	
	view->setImage({});
	
	for (CuttleImgDisplay * disp : displays) {
		delete disp;
	}
	displays.clear();
	
	for (CuttleCollection * col : collections) {
		delete col;
	}
	collections.clear();
	
	for (CuttleSet * set : sets) {
		delete set;
	}
	sets.clear();
	
	int res = resSpin->value();
	progress->setTextVisible(true);
	
	if (lastDir != dirstr || res != lastRes) {
		for (CuttleImg * img : imgs) {
			delete img;
		}
		imgs.clear();
		
		load_files = dir.entryInfoList(QDir::Files);
		begcur = 0;
		begmax = load_files.count();
		progress->setMinimum(0);
		progress->setMaximum(load_files.count());
		
		std::vector<std::thread *> workers {};
		for (unsigned int i = 0; i < std::thread::hardware_concurrency(); i++) {
			workers.push_back(new std::thread {load_work, progress, &imgs, res});
		}
		for (std::thread * th : workers) {
			th->join();
		}
		
		progress->setValue(load_files.count());
		progress->setFormat("DONE");
		
		lastDir = dirstr;
		lastRes = res;
	}
	
	sim_checks.clear();
	for (int ai = 0; ai < imgs.count(); ai++) {
		for (int bi = ai; bi < imgs.count(); bi++) {
			sim_checks.push_back({ai, bi});
		}
	}
	
	begcur = 0;
	begmax = sim_checks.size();
	
	progress->setMinimum(0);
	progress->setMaximum(begmax);
	
	progress->setFormat("CALCULATING SIMILARITY: %p%");
	
	double thresh = threshSpin->value();
	
	
	std::vector<std::thread *> workers {};
	for (unsigned int i = 0; i < std::thread::hardware_concurrency(); i++) {
		workers.push_back(new std::thread {sim_work, progress, &imgs, &sets, res, thresh});
	}
	for (std::thread * th : workers) {
		th->join();
	}
	
	/*
	int ccount = 0;
	for (int ai = 0; ai < imgs.count(); ai++) {
		for (int bi = ai; bi < imgs.count(); bi++) {
			progress->setValue(ccount++);
			
			if (ai == bi) continue;
			CuttleImg * ac = imgs[ai];
			CuttleImg * bc = imgs[bi];
			if (ac->set != nullptr && ac->set == bc->set) continue;
			
			float cmp = CuttleImg::compare(*ac, *bc, res);
			if (cmp < thresh) continue;
			
			if (ac->set && bc->set) {
				CuttleSet * delset = bc->set;
				ac->set->merge(*delset);
				sets.removeAll(delset);
				delete delset;
			} else if (ac->set) {
				ac->set->add(bc);
			} else if (bc->set) {
				bc->set->add(ac);
			} else {
				CuttleSet * s = new CuttleSet {};
				s->add(ac);
				s->add(bc);
				sets.append(s);
			}
		}
	}
	*/
	
	progress->setValue(begmax);
	progress->setFormat("GENERATING THUMBNAILS: %p%");
	
	int ccount = 0;
	progress->setMaximum(sets.size());
	for (CuttleSet * set : sets) {
		progress->setValue(ccount++);
		CuttleCollection * col = new CuttleCollection {this, set};
		collectionLayout->addWidget(col);
		collections.append(col);
	}
	progress->setFormat("DONE");
	progress->setValue(ccount);
}

// ================================================================
// ----------------------------------------------------------------
// ================================================================

void CuttleCore::beginDialog() {
	QString dir = QFileDialog::getExistingDirectory(this, "Select Directory", lastDir.isNull() ? QDir::currentPath() : lastDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!dir.isNull()) begin(dir);
}

void CuttleCore::recalculate() {
	if (lastDir.isNull()) return;
	begin(lastDir);
}

void CuttleCore::setName(QString str) {
	curName->setText(str);
}

void CuttleCore::removeCurrent() {
	if (!curCollection || !curImg) return;
	curCollection->set->imgs.removeAll(curImg->img);
	if(!curCollection->updateCount()) {
		delete curCollection;
		collections.removeAll(curCollection);
		curCollection = nullptr;
	}
	this->showCollection(curCollection);
}

void CuttleCore::deleteCurrent() {
	if (!curCollection || !curImg) return;
	QString deleteFile = curImg->img->file.canonicalFilePath();
	if (QMessageBox::warning(this, "File Deletion", QString("This will delete the image from disk. Are you sure you want to do this?\n%1").arg(deleteFile), QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes) return;
	this->removeCurrent();
	QFile::remove(deleteFile);
}
