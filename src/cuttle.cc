#include "cuttle.hh"

#include <asterid/strops.hh>
#include "rwsl.hh"

#include <atomic>
#include <thread>
#include <vector>

#include <QtWidgets>
#include <QImage>
#include <QImageReader>
#include <QDebug>

//#define DEFAULT_THRESHOLD 0.985
#define DEFAULT_THRESHOLD 0.8

CuttleCore::CuttleCore() : QMainWindow() {
	
	builder = new CuttleBuilder {this};
	builder->hide();
	processor = new CuttleProcessor {this};
	
	QWidget * mainCont = new QWidget {this};
	this->setCentralWidget(mainCont);
	
	QGridLayout * mainLayout = new QGridLayout {mainCont};
	
	QWidget * viewCont = new QWidget {mainCont};
	QHBoxLayout * viewLayout = new QHBoxLayout {viewCont};
	viewLayout->setMargin(0);
	
	QScrollArea * leftListArea = new QScrollArea {viewCont};
	QWidget * leftListWidget = new QWidget {leftListArea};
	leftListArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	leftListArea->setWidgetResizable(true);
	leftListArea->setMinimumWidth(LEFT_COLUMN_SIZE);
	leftListArea->setMaximumWidth(LEFT_COLUMN_SIZE);
	leftListWidget->setMinimumWidth(LEFT_COLUMN_SIZE - 20);
	leftListWidget->setMaximumWidth(LEFT_COLUMN_SIZE - 20);
	leftListArea->setWidget(leftListWidget);
	QVBoxLayout * leftListLayout = new QVBoxLayout {leftListWidget};
	viewLayout->addWidget(leftListArea);
	
	QScrollArea * rightListArea = new QScrollArea {viewCont};
	QWidget * rightListWidget = new QWidget {rightListArea};
	rightListArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	rightListArea->setWidgetResizable(true);
	rightListArea->setMinimumWidth(RIGHT_COLUMN_SIZE);
	rightListArea->setMaximumWidth(RIGHT_COLUMN_SIZE);
	rightListWidget->setMinimumWidth(RIGHT_COLUMN_SIZE - 20);
	rightListWidget->setMaximumWidth(RIGHT_COLUMN_SIZE - 20);
	rightListArea->setWidget(rightListWidget);
	QVBoxLayout * rightListLayout = new QVBoxLayout {rightListWidget};
	viewLayout->addWidget(rightListArea);
	
	QWidget * activeWidget = new QWidget {viewCont};
	QVBoxLayout * activeLayout = new QVBoxLayout {activeWidget};
	
	QWidget * activeCompWidget = new QWidget {activeWidget};
	QHBoxLayout * activeCompLayout = new QHBoxLayout {activeCompWidget};
	activeCompLayout->setMargin(0);
	
	activeLayout->addWidget(activeCompWidget);
	
	QPushButton * diffButton = new QPushButton {"Diff", viewCont};
	activeLayout->addWidget(diffButton);
	
	view = new ImageView {viewCont};
	view->setMinimumSize(400, 400);
	view->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	activeLayout->addWidget(view);
	
	activeLayout->setMargin(0);
	viewLayout->addWidget(activeWidget);
	
	mainLayout->addWidget(viewCont, 0, 0, 1, 1);
	
	QWidget * controlCont = new QWidget {mainCont};
	QHBoxLayout * controlLayout = new QHBoxLayout {controlCont};
	controlLayout->setMargin(0);
	
	QPushButton * newButton = new QPushButton {"New", controlCont};
	controlLayout->addWidget(newButton);
	
	QProgressBar * progress = new QProgressBar {controlCont};
	progress->setMinimum(0);
	progress->setMaximum(100);
	controlLayout->addWidget(progress);
	
	QPushButton * stopButton = new QPushButton {"Stop", controlCont};
	controlLayout->addWidget(stopButton);
	
	mainLayout->addWidget(controlCont, 1, 0, 1, 1);
	
	connect(builder, &CuttleBuilder::begin, processor, &CuttleProcessor::beginProcessing);
	connect(newButton, &QPushButton::pressed, builder, &CuttleBuilder::focus);
	connect(stopButton, &QPushButton::pressed, processor, [this](){this->processor->stop();});
	
	connect(processor, &CuttleProcessor::section, progress, [progress](QString str){
		progress->setFormat(str);
		progress->update();
	}, Qt::QueuedConnection);
	connect(processor, &CuttleProcessor::max, progress, [progress](int max){
		progress->setMaximum(max);
		progress->update();
	}, Qt::QueuedConnection);
	connect(processor, &CuttleProcessor::value, progress, [progress](int value){
		progress->setValue(value);
		progress->update();
	}, Qt::QueuedConnection);
	
	connect(processor, &CuttleProcessor::started, this, [=](){
		this->view->setImage({});
		leftListArea->setEnabled(false);
		rightListArea->setEnabled(false);
		newButton->setEnabled(false);
		
		for (CuttleLeftItem * item : leftList) {
			delete item;
		}
		leftList.clear();
		
		for (CuttleRightItem * item : rightList) {
			delete item;
		}
		rightList.clear();
		
		for (CuttleCompItem * item : compList) {
			delete item;
		}
		compList.clear();
	}, Qt::QueuedConnection);
	
	connect(processor, &CuttleProcessor::finished, this, [=](){
		leftListArea->setEnabled(true);
		rightListArea->setEnabled(true);
		newButton->setEnabled(true);
		
		for (CuttleSet const * set : processor->getSetsAboveThresh(DEFAULT_THRESHOLD)) {
			CuttleLeftItem * item = new CuttleLeftItem {leftListArea, set, processor};
			leftList.append(item);
			
			connect(item, &CuttleLeftItem::activated, this, [=](CuttleSet const * active_set) {
				auto comp_func = [=](CuttleSet const * set){
					for (auto * item : compList) {
						delete item;
					}
					compList.clear();
					
					diffButton->disconnect();
					connect(diffButton, &QPushButton::pressed, this, [=]() {
						QImage A = set->getImage();
						QImage B = active_set->getImage();
						if (A.size() != B.size()) B = B.scaled(A.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
						QImage C {A.size(), QImage::Format_RGB32};
						for (int y = 0; y < A.height(); y++) for (int x = 0; x < A.width(); x++) {
							QColor pA = A.pixel(x, y), pB = B.pixel(x, y), pC;
							pC.setRgb(qAbs(pA.red() - pB.red()), qAbs(pA.green() - pB.green()), qAbs(pA.blue() - pB.blue()));
							C.setPixel(x, y, pC.rgb());
						}
						view->setImage(C, ImageView::KEEP_FIT_FORCE);
					});
					
					CuttleCompInfo set_c, active_set_c;
					CuttleCompInfo::GetCompInfo(set, active_set, set_c, active_set_c);
					
					CuttleCompItem * itemL = new CuttleCompItem {activeCompWidget, active_set, active_set_c, processor};
					CuttleCompItem * itemR = new CuttleCompItem {activeCompWidget, set, set_c, processor};
					
					connect(itemL, &CuttleCompItem::view, this, [=](CuttleSet const * set){ view->setImage(set->getImage(), ImageView::KEEP_FIT_FORCE); });
					connect(itemR, &CuttleCompItem::view, this, [=](CuttleSet const * set){ view->setImage(set->getImage(), ImageView::KEEP_FIT_FORCE); });
					
					auto deleteme_func = [=](CuttleSet const * set) {
						QFile::remove(set->filename);
						processor->remove(set);
					};
					connect(itemL, &CuttleCompItem::delete_me, this, deleteme_func);
					connect(itemR, &CuttleCompItem::delete_me, this, deleteme_func);
					
					compList.append(itemL);
					compList.append(itemR);
					activeCompLayout->addWidget(itemL);
					activeCompLayout->addWidget(itemR);
					
					view->setImage(set->getImage(), ImageView::KEEP_FIT_FORCE);
				};
				for (CuttleRightItem * item : rightList) {
					delete item;
				}
				rightList.clear();
				for (CuttleSet const * set : processor->getSetsAboveThresh(active_set, DEFAULT_THRESHOLD)) {
					if (set->id == active_set->id) continue;
					CuttleRightItem * item = new CuttleRightItem {rightListWidget, set, active_set, processor};
					rightList.append(item);
					
					connect(item, &CuttleRightItem::activated, this, comp_func);
				}
				qSort(rightList.begin(), rightList.end(), [](CuttleRightItem const * A, CuttleRightItem const * B){return A->getValue() > B->getValue();});
				for (CuttleRightItem * item : rightList) {
					rightListLayout->addWidget(item);
				}
				comp_func(rightList[0]->set);
				view->setImage(active_set->getImage(), ImageView::KEEP_FIT_FORCE);
			});
		}
		qSort(leftList.begin(), leftList.end(), [](CuttleLeftItem const * A, CuttleLeftItem const * B){return A->getHigh() > B->getHigh();});
		for (CuttleLeftItem * item : leftList) {
			leftListLayout->addWidget(item);
		}
		
	}, Qt::QueuedConnection);
}

void CuttleCompInfo::GetCompInfo(CuttleSet const * A, CuttleSet const * B, CuttleCompInfo & Ac, CuttleCompInfo & Bc) {
	auto Aimg = A->getImage();
	auto Bimg = B->getImage();
	Ac.equal = Bc.equal = (Aimg == Bimg);
	
	auto Asize = A->fi.size();
	auto Bsize = B->fi.size();
	if (Asize == Bsize) Ac.size = Bc.size = status::same;
	else if (Asize > Bsize) {
		Ac.size = status::high;
		Bc.size = status::low;
	} else {
		Ac.size = status::low;
		Bc.size = status::high;
	}
	
	auto Adims = A->get_size();
	auto Bdims = B->get_size();
	auto Adimsum = Adims.width() * Adims.height();
	auto Bdimsum = Bdims.width() * Bdims.height();
	
	if (Adimsum == Bdimsum) Ac.dims = Bc.dims = status::same;
	else if (Adimsum > Bdimsum) {
		Ac.dims = status::high;
		Bc.dims = status::low;
	} else {
		Ac.dims = status::low;
		Bc.dims = status::high;
	}
	
	auto Adate = A->fi.lastModified();
	auto Bdate = B->fi.lastModified();
	if (Adate == Bdate) Ac.date = Bc.date = status::same;
	else if (Adate > Bdate) {
		Ac.date = status::high;
		Bc.date = status::low;
	} else {
		Ac.date = status::low;
		Bc.date = status::high;
	}
}
