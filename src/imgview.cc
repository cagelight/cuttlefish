#include <QtGui>
#include "imgview.hh"

ImageView::ImageView(QWidget *parent, QImage image) : QWidget(parent), view(image) {
	this->setAttribute(Qt::WA_AcceptTouchEvents, true);
	qRegisterMetaType<ZKEEP>("ZKEEP");
	mouseHider->setSingleShot(true);
	QObject::connect(mouseHider, SIGNAL(timeout()), this, SLOT(hideMouse()));
	this->setMouseTracking(true);
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QObject::connect(this, SIGNAL(bilComplete()), this, SLOT(update()));
// 	bilWorker = new std::thread(&ImageView::bilRun, this);
}

ImageView::~ImageView() {
// 	bilGood.store(false);
// 	if (bilWorker->joinable()) bilWorker->join();
// 	delete bilWorker;
}

QSize ImageView::sizeHint() const {
	return view.size();
}

void ImageView::paintEvent(QPaintEvent *QPE) {
	QPainter paint(this);
	paint.fillRect(QPE->rect(), QBrush(QColor(0, 0, 0)));
// 	viewLock.read_access();
	if (this->width() > 0 && this->height() > 0 && view.width() > 0 && view.height() > 0) {
		this->calculateView();
		QSize drawSize;
// 		drawLock.read_access();
		if (paintCompletePartial) {
			drawSize = this->size();
		} else if ((partRect.width() < this->width() && partRect.height() < this->height())) {
			drawSize = partRect.size() / zoom;
		} else {
			drawSize = partRect.size().scaled(this->size(), Qt::KeepAspectRatio);
		}
// 		drawLock.read_done();
// 		drawLock.write_lock();
		drawRect.setX((int)((this->width() - drawSize.width()) / 2.0f));
		drawRect.setY((int)((this->height() - drawSize.height()) / 2.0f));
		drawRect.setSize(drawSize);
// 		if (drawRect == bilDraw && partRect == bilPart && view == bilCompare) paint.drawImage(drawRect, bilRaster);
		/*else*/ paint.drawImage(drawRect, view, partRect);
// 		drawLock.write_unlock();
	}
// 	viewLock.read_done();
	QWidget::paintEvent(QPE);
}

void ImageView::resizeEvent(QResizeEvent *QRE) {
	this->calculateZoomLevels();
	QWidget::resizeEvent(QRE);
}

void ImageView::wheelEvent(QWheelEvent *QWE) {
	if (QWE->orientation() == Qt::Vertical) {
		QWE->accept();
		this->setZoom((1.0f - QWE->delta() / 360.0f / 3.0f) * zoom, QPointF(QWE->pos().x() / (float)this->width() , QWE->pos().y() / (float)this->height()));
		this->update();
	}
	QWidget::wheelEvent(QWE);
}

void ImageView::mousePressEvent(QMouseEvent *QME) {
	this->showMouse();
	if (QME->button() == Qt::LeftButton) {
		QME->accept();
		this->setFocus();
		this->mouseMoving = true;
		this->prevMPos = QME->pos();
	}
	if (QME->button() == Qt::MiddleButton) {
		this->setZoom(1.0f, QPointF(QME->pos().x() / (float)this->width() , QME->pos().y() / (float)this->height()));
		this->update();
	}
	QWidget::mousePressEvent(QME);
}

void ImageView::mouseReleaseEvent(QMouseEvent *QME) {
	if (QME->button() == Qt::LeftButton) {
		this->mouseMoving = false;
	}
	QWidget::mouseReleaseEvent(QME);
}

void ImageView::mouseMoveEvent(QMouseEvent *QME) {
	this->showMouse();
	if (mouseMoving && !touchOverride) {
		if (keep == KEEP_EXPANDED || keep == KEEP_EQUAL) keep = KEEP_NONE;
		QPointF nPosAdj = ((QPointF)prevMPos - (QPointF)QME->pos()) * zoom;
		QPointF prevView = viewOffset;
		this->viewOffset.rx() += nPosAdj.x();
		this->viewOffset.ry() += nPosAdj.y();
		this->calculateView();
		if (viewOffset.toPoint() != prevView.toPoint()) {
			this->update();
		}
		this->prevMPos = QME->pos();
	}
	QWidget::mouseMoveEvent(QME);
}

bool ImageView::event(QEvent * ev) {
	
	if (ev->type() == QInputEvent::TouchBegin) {
		ev->accept();
		touchOverride = true;
	} else if (ev->type() == QInputEvent::TouchEnd) {
		ev->accept();
		touchOverride = false;
	} else if (ev->type() == QInputEvent::TouchUpdate) {
		QTouchEvent * tev = dynamic_cast<QTouchEvent *>(ev);
		tev->accept();
		
		
		QPointF centerNow, centerLast;
		
		for (auto & tp : tev->touchPoints()) {
			centerNow += tp.pos();
			centerLast += tp.lastPos();
		}
		centerNow /= tev->touchPoints().length();
		centerLast /= tev->touchPoints().length();
		
		qreal pinch = 0;
		for (auto & tp : tev->touchPoints()) {
			pinch += QVector2D::dotProduct(QVector2D(tp.pos() - tp.lastPos()).normalized(), QVector2D(centerNow - tp.pos()).normalized());
		}
		
		this->setZoom((1.0f - pinch / 50000) * zoom, QPointF(centerNow.x() / (float)this->width() , centerNow.y() / (float)this->height()));
		
		qDebug() << "Touch Center:" << centerNow << "Last:" << centerLast; 
		QPointF centerDelta = (centerNow - centerLast) * zoom;
		
		this->viewOffset.rx() -= centerDelta.x();
		this->viewOffset.ry() -= centerDelta.y();
		this->calculateView();
		this->update();
		
	}
	
	return QWidget::event(ev);
}

void ImageView::setImage(QImage newView, ZKEEP keepStart) {
	this->keep = keepStart;
// 	viewLock.write_lock();
	if (this->view != newView) {
		this->view = newView;
// 		viewLock.write_unlock();
		this->update();
	}// else viewLock.write_unlock();
}

void ImageView::setImagePreserve(QImage newView) {
	//viewLock.write_lock();
	if (this->view != newView) {
		this->view = newView;
		//viewLock.write_unlock();
		this->update();
	}// else viewLock.write_unlock();
}

QImage ImageView::getImageOfView() {
	//viewLock.read_access();
	//drawLock.read_access();
	QImage part = view.copy(partRect);
	//drawLock.read_done();
	//viewLock.read_done();
	return part;
}

void ImageView::setZoom(qreal nZoom, QPointF focus) {
	if (nZoom < zoomMin) nZoom = zoomMin;
	if (nZoom > zoomMax) nZoom = zoomMax;
	if (zoom != nZoom) {
		QSize sizeZ = this->size() * zoom;
		QSize sizeNZ = this->size() * nZoom;
		QSize sizeZV = this->view.size();
		if (sizeZV.width() > sizeZ.width()) {
			sizeZV.setWidth(sizeZ.width());
		}
		if (sizeZV.height() > sizeZ.height()) {
			sizeZV.setHeight(sizeZ.height());
		}
		float xmod = (sizeZV.width() - sizeNZ.width()) * focus.x();
		float ymod = (sizeZV.height() - sizeNZ.height()) * focus.y();
		this->viewOffset.rx() += xmod;
		this->viewOffset.ry() += ymod;
		zoom = nZoom;
		keep = KEEP_NONE;
	} else if (keep == KEEP_NONE && zoom == nZoom && zoom != zoomMin) {
		keep = KEEP_FIT;
	}
}

void ImageView::centerView() {
	QSize sizeZ = this->size() * zoomMax;
	QSize sizeNZ = this->size();
	QSize sizeZV = this->view.size();
	if (sizeZV.width() > sizeZ.width()) {
		sizeZV.setWidth(sizeZ.width());
	}
	if (sizeZV.height() > sizeZ.height()) {
		sizeZV.setHeight(sizeZ.height());
	}
	float xmod = (sizeZV.width() - sizeNZ.width()) * 0.5f;
	float ymod = (sizeZV.height() - sizeNZ.height()) * 0.5f;
	this->viewOffset.setX(xmod);
	this->viewOffset.setY(ymod);
}

void ImageView::calculateZoomLevels() {
	float xmax = view.width() / (float) this->width();
	float ymax = view.height() / (float) this->height();
	if (xmax > ymax) {
		zoomMax = xmax;
		zoomExp = ymax;
	} else {
		zoomExp = xmax;
		zoomMax = ymax;
	}
	zoomFit = zoomMax;
	if (zoomMax < 1.0f) zoomMax = 1.0f;
}

void ImageView::calculateView() {
	this->calculateZoomLevels();
	if (zoom == zoomMax && keep == KEEP_NONE) keep = KEEP_FIT;
	switch (keep) {
	case KEEP_NONE:
		break;
	case KEEP_FIT:
		zoom = zoomMax;
		this->viewOffset = QPointF(0, 0);
		break;
	case KEEP_FIT_FORCE:
		zoom = zoomFit;
		this->viewOffset = QPointF(0, 0);
		break;
	case KEEP_EXPANDED:
	{
		QSize mScr = this->size().scaled(this->view.size(), Qt::KeepAspectRatio);
		this->viewOffset.setX((this->view.size().width() - mScr.width())/ 2.0f);
		this->viewOffset.setY((this->view.size().height() - mScr.height())/ 2.0f);
		zoom = zoomExp;
	} break;
	case KEEP_EQUAL:
		this->zoom = 1.0f;
		this->centerView();
		break;
	}
	QSize partSize = this->size() * zoom;
	this->paintCompletePartial = true;
	if (partSize.width() > view.width()) {
		partSize.setWidth(view.width());
		this->paintCompletePartial = false;
	}
	if (partSize.height() > view.height()) {
		partSize.setHeight(view.height());
		this->paintCompletePartial = false;
	}
	if (viewOffset.x() > this->view.width() - partSize.width()) {
		viewOffset.setX(this->view.width() - partSize.width());
	}
	if (viewOffset.y() > this->view.height() - partSize.height()) {
		viewOffset.setY(this->view.height() - partSize.height());
	}
	if (viewOffset.x() < 0) {
		viewOffset.setX(0);
	}
	if (viewOffset.y() < 0) {
		viewOffset.setY(0);
	}
	//drawLock.write_lock();
	partRect = QRect(viewOffset.toPoint(), partSize);
	//drawLock.write_unlock();
}

/*
void ImageView::bilRun() {
	while(bilGood) {
		bool doBil = false;
		viewLock.read_access();
		drawLock.read_access();
		if (bilPart != partRect || bilDraw != drawRect || bilCompare != view) doBil = true;
		drawLock.read_done();
		viewLock.read_done();
		if (doBil) {
			emit bilProc();
			viewLock.read_access();
			QImage rCompare = view;
			drawLock.read_access();
			QRect rPart = partRect;
			QRect rDraw = drawRect;
			drawLock.read_done();
			QImage raster;
			if(rPart == view.rect()) {
				raster = view;
			} else {
				raster = view.copy(rPart);
			}
			QSize rSize = rDraw.size();
			viewLock.read_done();
			raster = raster.scaled(rSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			drawLock.write_lock();
			bilRaster = raster;
			bilPart = rPart;
			bilDraw = rDraw;
			bilCompare = rCompare;
			drawLock.write_unlock();
			emit bilComplete();
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
		}
	}
}
*/
