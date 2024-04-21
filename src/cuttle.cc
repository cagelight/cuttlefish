#include "cuttle.hh"

#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include <QtWidgets>
#include <QImage>
#include <QDebug>
#include <QShortcut>

CuttleCore::CuttleCore() : QMainWindow() {
	
	builder = new CuttleBuilder {this};
	builder->hide();
	processor = new CuttleProcessor {this};
	
	QWidget * mainCont = new QWidget {this};
	this->setCentralWidget(mainCont);
	
	QGridLayout * mainLayout = new QGridLayout {mainCont};
	
	QWidget * viewCont = new QWidget {mainCont};
	QHBoxLayout * viewLayout = new QHBoxLayout {viewCont};
	viewLayout->setContentsMargins(0, 0, 0, 0);
	
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
	activeCompLayout->setContentsMargins(0, 0, 0, 0);
	
	activeLayout->addWidget(activeCompWidget);
	
	view = new ImageView {viewCont};
	view->setKeepState(ImageView::KEEP_FIT_FORCE);
	view->setMinimumSize(400, 400);
	view->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	activeLayout->addWidget(view);
	
	QWidget * imgControlCont = new QWidget {viewCont};
	QHBoxLayout * imgControlLayout = new QHBoxLayout {imgControlCont};
	imgControlLayout->setAlignment(Qt::AlignTop);
	imgControlLayout->setContentsMargins(0, 0, 0, 0);
	
	// Diff
	QWidget * diffWidget = new QWidget { imgControlCont };
	QVBoxLayout * diffLayout = new QVBoxLayout { diffWidget };
	diffLayout->setContentsMargins(0, 0, 0, 0);
	QPushButton * diffButton = new QPushButton { "Diff", diffWidget };
	diffLayout->addWidget(diffButton);
	QSlider * diffSlider = new QSlider { diffWidget };
	diffSlider->setMinimum(1);
	diffSlider->setMaximum(255);
	diffSlider->setOrientation(Qt::Horizontal);
	diffSlider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	diffLayout->addWidget(diffSlider);
	imgControlLayout->addWidget(diffWidget);
	
	QShortcut * diffShort = new QShortcut { QKeySequence { tr("Ctrl+D") }, diffWidget };
	connect(diffShort, &QShortcut::activated, diffWidget, [diffButton, diffSlider](){
		diffSlider->setValue(1);
		diffButton->click();
	});
	
	QShortcut * diffMaxShort = new QShortcut { QKeySequence { tr("Shift+D") }, diffWidget };
	connect(diffMaxShort, &QShortcut::activated, diffWidget, [diffButton, diffSlider](){
		diffSlider->setValue(255);
		diffButton->click();
	});
	
	// Ignore
	QPushButton * ignoreButton = new QPushButton {"Ignore", imgControlCont};
	ignoreButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	imgControlLayout->addWidget(ignoreButton);
	
	QShortcut * ignoreShort = new QShortcut { QKeySequence { tr("Ctrl+I") }, ignoreButton };
	connect(ignoreShort, &QShortcut::activated, ignoreButton, &QPushButton::click);
	
	// Alternate Sides
	QPushButton * alternateView = new QPushButton {"Alternate", imgControlCont};
	alternateView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	imgControlLayout->addWidget(alternateView);
	
	connect(alternateView, &QPushButton::clicked, this, [this](){
		if (!cItemA) return;
		cItemA = (cItemA == cItemL) ? cItemR : cItemL;
		cItemA->view();
	});
	
	// Delete Current
	QPushButton * deleteCurrent = new QPushButton {"Delete Current", imgControlCont};
	deleteCurrent->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	imgControlLayout->addWidget(deleteCurrent);
	
	connect(deleteCurrent, &QPushButton::clicked, this, [this](){
		if (!cItemA) return;
		cItemA->deleteMe();
	});
	
	// Save Viewport
	QPushButton * saveView = new QPushButton {"Save Current Viewport", imgControlCont};
	saveView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	imgControlLayout->addWidget(saveView);
	
	connect(saveView, &QPushButton::clicked, this, [this](){
		QImage img = view->getImageOfView();
		QString loc = QFileDialog::getSaveFileName(this, "Save Image View", "view.png", tr("Portable Network Graphics (*.png)"));
		if (!loc.isEmpty()) {
			img.save(loc, nullptr, 100);
		}
	});
	
	activeLayout->addWidget(imgControlCont);
	
	activeLayout->setContentsMargins(0, 0, 0, 0);
	viewLayout->addWidget(activeWidget);
	
	mainLayout->addWidget(viewCont, 0, 0, 1, 1);
	
	QWidget * controlCont = new QWidget {mainCont};
	QHBoxLayout * controlLayout = new QHBoxLayout {controlCont};
	controlLayout->setContentsMargins(0, 0, 0, 0);
	
	//QPushButton * raiButton = new QPushButton {"RAI", controlCont};
	//raiButton->setToolTip("Remove All Identical: Will remove all but one identical images in a group, the smallest one will be kept.");
	//controlLayout->addWidget(raiButton);
	
	QPushButton * newButton = new QPushButton {"New", controlCont};
	controlLayout->addWidget(newButton);
	
	QDoubleSpinBox * threshSpin = new QDoubleSpinBox {controlCont};
	threshSpin->setSingleStep(0.001);
	threshSpin->setMinimum(0);
	threshSpin->setMaximum(0.999);
	threshSpin->setDecimals(3);
	threshSpin->setValue(0.850);
	controlLayout->addWidget(threshSpin);
	
	QPushButton * threshButton = new QPushButton {"Refresh", controlCont};
	controlLayout->addWidget(threshButton);
	
	QProgressBar * progress = new QProgressBar {controlCont};
	progress->setMinimum(0);
	progress->setMaximum(100);
	controlLayout->addWidget(progress);
	
	QPushButton * stopButton = new QPushButton {"Stop", controlCont};
	controlLayout->addWidget(stopButton);
	
	mainLayout->addWidget(controlCont, 1, 0, 1, 1);
	
	QShortcut * shortL = new QShortcut(QKeySequence(tr("1", "View Left")), this);
	QShortcut * shortR = new QShortcut(QKeySequence(tr("2", "View Right")), this);
	
	connect(builder, &CuttleBuilder::begin, processor, &CuttleProcessor::beginProcessing);
	//connect(raiButton, &QPushButton::clicked, processor, &CuttleProcessor::remove_all_idential);
	connect(newButton, &QPushButton::clicked, builder, &CuttleBuilder::focus);
	connect(stopButton, &QPushButton::clicked, processor, [this](){this->processor->stop();});
	
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
	
	auto startUIFunc =  [=](){
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
		
		if (cItemL) delete cItemL;
		if (cItemR) delete cItemR;
		cItemL = cItemR = cItemA = nullptr;
	};
	
	auto finishUIFunc = [=](){
		leftListArea->setEnabled(true);
		rightListArea->setEnabled(true);
		newButton->setEnabled(true);
		
		for (CuttleSet const * set : processor->getSetsAboveThresh(threshSpin->value())) {
			
			CuttleLeftItem * item = new CuttleLeftItem {leftListArea, set, processor};
			leftList.append(item);
			
			connect(item, &CuttleLeftItem::activated, this, [=](CuttleSet const * active_set) {
				auto comp_func = [=](CuttleSet const * set){
					if (cItemL) delete cItemL;
					if (cItemR) delete cItemR;
					cItemL = cItemR = cItemA = nullptr;
					
					auto iRa = std::async(std::launch::async, [&set](){ return set->getImage(); });
					QImage iL = active_set->getImage();
					QImage iR = iRa.get();
					
					// ================================
					
					diffButton->disconnect();
					connect(diffButton, &QPushButton::clicked, this, [=]() {
						QImage A = iL, B = iR;
						if (A.size() != B.size()) {
							auto As = A.width() * A.height(), Bs = B.width() * B.height();
							if (As > Bs)
								B = B.scaled(A.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
							else 
								A = A.scaled(B.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
						}
						
						QImage C {A.size(), QImage::Format_RGB32};
						
						struct diff_state_t {
							int const lines;
							std::atomic_int line;
						} diff_state {
							.lines = A.height(),
							.line = 0,
						};
						
						float const mult = 255.0f / (256 - diffSlider->value());
						
						auto thread_func = [&diff_state, &A, &B, &C, mult] () {
							int y;
							while ((y = diff_state.line.fetch_add(1)) < diff_state.lines) {
								QRgb * line_data = reinterpret_cast<QRgb *>(C.scanLine(y));
								for (int x = 0; x < A.width(); x++) {
									QColor pA = A.pixel(x, y), pB = B.pixel(x, y), pC;
									pC.setRgbF(qAbs(pA.redF() - pB.redF()) * mult, qAbs(pA.greenF() - pB.greenF()) * mult, qAbs(pA.blueF() - pB.blueF()) * mult);
									line_data[x] = pC.rgb();
								}
							}
						};
						
						std::vector<std::thread> threads;
						for (size_t i = 0; i < std::thread::hardware_concurrency(); i++) threads.emplace_back(thread_func);
						for (auto & t : threads) t.join();
						threads.clear();
						
						view->setImagePreserve(C);
					});
					
					// ================================
					
					CuttleCompInfo set_c, active_set_c;
					CuttleCompInfo::GetCompInfo(set, active_set, set_c, active_set_c);
					
					cItemL = new CuttleCompItem {activeCompWidget, active_set, active_set_c, processor};
					cItemR = new CuttleCompItem {activeCompWidget, set, set_c, processor};
					
					cItemA = cItemL;
					
					shortL->disconnect();
					shortR->disconnect();
					connect(shortL, &QShortcut::activated, this, [this, iL](){ view->setImagePreserve(iL); });
					connect(shortR, &QShortcut::activated, this, [this, iR](){ view->setImagePreserve(iR); });
					
					connect(cItemL, &CuttleCompItem::view, this, [this, iL](){ view->setImagePreserve(iL); });
					connect(cItemR, &CuttleCompItem::view, this, [this, iR](){ view->setImagePreserve(iR); });
					
					auto deleteme_func = [=](CuttleSet const * set) {
						QFile::remove(set->filename);
						processor->remove(set);
					};
					connect(cItemL, &CuttleCompItem::delete_me, this, deleteme_func);
					connect(cItemR, &CuttleCompItem::delete_me, this, deleteme_func);
					
					ignoreButton->disconnect();
					connect(ignoreButton, &QPushButton::clicked, this, [=]() {
						processor->remove(set, active_set);
					});
					
					activeCompLayout->addWidget(cItemL);
					activeCompLayout->addWidget(cItemR);
					
					view->setImagePreserve(set->getImage());
				};
				for (CuttleRightItem * item : rightList) {
					delete item;
				}
				rightList.clear();
				for (CuttleSet const * set : processor->getSetsAboveThresh(active_set, threshSpin->value())) {
					if (set->id == active_set->id) continue;
					CuttleRightItem * item = new CuttleRightItem {rightListWidget, set, active_set, processor};
					rightList.append(item);
					
					connect(item, &CuttleRightItem::activated, this, comp_func);
				}
				std::sort(rightList.begin(), rightList.end(), [](CuttleRightItem const * A, CuttleRightItem const * B){return A->getValue() > B->getValue();});
				for (CuttleRightItem * item : rightList) {
					rightListLayout->addWidget(item);
				}
				comp_func(rightList[0]->set);
				view->setImage(active_set->getImage(), ImageView::KEEP_FIT_FORCE);
			});
		}
		std::sort(leftList.begin(), leftList.end(), [](CuttleLeftItem const * A, CuttleLeftItem const * B){return A->getHigh() > B->getHigh();});
		for (CuttleLeftItem * item : leftList) {
			leftListLayout->addWidget(item);
		}
		
		// TODO -- Setting
		if (leftList.size()) leftList[0]->activate();
		
	};
	
	connect(processor, &CuttleProcessor::started, this, startUIFunc, Qt::QueuedConnection);
	connect(processor, &CuttleProcessor::finished, this, finishUIFunc, Qt::QueuedConnection);
	
	connect(threshButton, &QPushButton::clicked, this, [=](){
		startUIFunc();
		finishUIFunc();
	});
}

void CuttleCompInfo::GetCompInfo(CuttleSet const * A, CuttleSet const * B, CuttleCompInfo & Ac, CuttleCompInfo & Bc) {
	
	if (A->img_size == B->img_size) {
		Ac.equal = Bc.equal = (A->img_hash == B->img_hash);
	} else {
		Ac.equal = Bc.equal = false;
	}
	
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
