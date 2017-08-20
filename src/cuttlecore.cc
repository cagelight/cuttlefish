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
	
	activeLayout->addWidget(activeCompWidget);
	
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
		
		for (CuttleSet const * set : processor->getSetsAboveThresh(0.95)) {
			CuttleLeftItem * item = new CuttleLeftItem {leftListArea, *set, processor};
			leftList.append(item);
			
			connect(item, &CuttleLeftItem::activated, this, [=](CuttleSet const * active_set){
				auto comp_func = [=](CuttleSet const * set){
					for (auto * item : compList) {
						delete item;
					}
					compList.clear();
					
					CuttleCompItem * itemL = new CuttleCompItem {activeCompWidget, *active_set, *set, processor};
					CuttleCompItem * itemR = new CuttleCompItem {activeCompWidget, *set, *active_set, processor};
					
					connect(itemL, &CuttleCompItem::view, this, [=](CuttleSet const * set){ view->setImage(set->getImage()); });
					connect(itemR, &CuttleCompItem::view, this, [=](CuttleSet const * set){ view->setImage(set->getImage()); });
					
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
					
					view->setImage(set->getImage());
				};
				for (CuttleRightItem * item : rightList) {
					delete item;
				}
				rightList.clear();
				for (CuttleSet const * set : processor->getSetsAboveThresh(active_set, 0.95)) {
					if (set->id == active_set->id) continue;
					CuttleRightItem * item = new CuttleRightItem {rightListWidget, *set, *active_set, processor};
					rightList.append(item);
					
					connect(item, &CuttleRightItem::activated, this, comp_func);
				}
				qSort(rightList.begin(), rightList.end(), [](CuttleRightItem const * A, CuttleRightItem const * B){return A->getValue() > B->getValue();});
				for (CuttleRightItem * item : rightList) {
					rightListLayout->addWidget(item);
				}
				comp_func(rightList[0]->set);
				view->setImage(active_set->getImage());
			});
		}
		qSort(leftList.begin(), leftList.end(), [](CuttleLeftItem const * A, CuttleLeftItem const * B){return A->getHigh() > B->getHigh();});
		for (CuttleLeftItem * item : leftList) {
			leftListLayout->addWidget(item);
		}
		
	}, Qt::QueuedConnection);
}
