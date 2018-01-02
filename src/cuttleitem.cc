#include "cuttle.hh"

#include <QtWidgets>

CuttleLeftItem::CuttleLeftItem(QWidget * parent, CuttleSet const * set, CuttleProcessor * proc) : QFrame(parent), set(set), proc(proc) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	QGridLayout * layout = new QGridLayout {this};
	
	QLabel * nameLabel = new QLabel { QFileInfo(set->filename).fileName(), this };
	nameLabel->setMinimumWidth(LEFT_COLUMN_SIZE - 50);
	nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	QFontMetrics metrics { nameLabel->font() };
	layout->addWidget(nameLabel, 0, 0, 1, 1);
	nameLabel->setText( metrics.elidedText(nameLabel->text(), Qt::ElideMiddle, nameLabel->width()) );
	
	QWidget * lowerWidget = new QWidget {this};
	QHBoxLayout * lowerLayout = new QHBoxLayout {lowerWidget};
	
	high = 0;
	for (CuttleSet const & cset : proc->getSets()) {
		if (cset.id == set->id) continue;
		double v = proc->getMatchData(set, &cset);
		if (v > high) high = v;
	}
	
	QPushButton * activateButton = new QPushButton {"GO"};
	activateButton->setMinimumWidth(15);
	activateButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(activateButton, &QPushButton::pressed, this, [=](){emit activated(set);});
	lowerLayout->addWidget(activateButton);
	
	QLabel * highestMatchLabel = new QLabel {QString("High: %1").arg(high), lowerWidget};
	lowerLayout->addWidget(highestMatchLabel);
	
	lowerLayout->setMargin(0);
	layout->addWidget(lowerWidget, 1, 0, 1, 1);
	
	connect(this, &CuttleLeftItem::recalculateHigh, this, [=](){
		high = 0;
		for (CuttleSet const & cset : proc->getSets()) {
			if (cset.id == set->id) continue;
			double v = proc->getMatchData(set, &cset);
			if (v > high) high = v;
		}
	});
}

CuttleRightItem::CuttleRightItem(QWidget * parent, CuttleSet const * set, CuttleSet const * other, CuttleProcessor * proc) : QFrame(parent), set(set) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	QGridLayout * layout = new QGridLayout {this};
	
	QLabel * nameLabel = new QLabel { QFileInfo(set->filename).fileName(), this };
	nameLabel->setMinimumWidth(RIGHT_COLUMN_SIZE - 50);
	nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	QFontMetrics metrics { nameLabel->font() };
	layout->addWidget(nameLabel, 0, 0, 1, 1);
	nameLabel->setText( metrics.elidedText(nameLabel->text(), Qt::ElideMiddle, nameLabel->width()) );
	
	QWidget * lowerWidget = new QWidget {this};
	QHBoxLayout * lowerLayout = new QHBoxLayout {lowerWidget};
	
	value = proc->getMatchData(set, other);
	
	QPushButton * activateButton = new QPushButton {"GO"};
	activateButton->setMinimumWidth(15);
	activateButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(activateButton, &QPushButton::pressed, this, [=](){emit activated(set);});
	lowerLayout->addWidget(activateButton);
	
	QLabel * highestMatchLabel = new QLabel {QString("Value: %1").arg(value), lowerWidget};
	lowerLayout->addWidget(highestMatchLabel);
	
	lowerLayout->setMargin(0);
	layout->addWidget(lowerWidget, 1, 0, 1, 1);
}

static QString readable_file_size(qint64 b) {
	if (b < 2048) return QString {"%1 B"}.arg(b);
	if (b < 2097152) return QString {"%1 KiB"}.arg(b / 1024.0);
	else return QString {"%1 MiB"}.arg(b / 1048576.0);
}

CuttleCompItem::CuttleCompItem(QWidget * parent, CuttleSet const * set, CuttleCompInfo const & comp, CuttleProcessor * proc) : QFrame(parent), set(set) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	QGridLayout * layout = new QGridLayout {this};
	
	QLabel * nameLabel = new QLabel { QFileInfo(set->filename).fileName(), this };
	layout->addWidget(nameLabel, 0, 0, 1, 1);
	
	QWidget * infoWidget = new QWidget {this};
	QHBoxLayout * infoLayout = new QHBoxLayout {infoWidget};
	
	QLabel * eqLabel = new QLabel {QString{ "<font color='%2'>%1</font>" }.arg(comp.equal ? "EQUAL" : "NOT EQUAL").arg(comp.equal ? "green" : "yellow"), infoWidget};
	infoLayout->addWidget(eqLabel);
	
	char const * color;
	switch (comp.size) {
		case CuttleCompInfo::status::high:
			color = "green";
			break;
		case CuttleCompInfo::status::same:
			color = "yellow";
			break;
		case CuttleCompInfo::status::low:
			color = "red";
			break;
	}
	
	QLabel * sizeLabel = new QLabel {QString{ "<font color='%2'>%1</font>" }.arg(readable_file_size(set->fi.size())).arg(color), infoWidget};
	infoLayout->addWidget(sizeLabel);
	
	switch (comp.dims) {
		case CuttleCompInfo::status::high:
			color = "green";
			break;
		case CuttleCompInfo::status::same:
			color = "yellow";
			break;
		case CuttleCompInfo::status::low:
			color = "red";
			break;
	}
	
	QSize dimsA = set->get_size();
	QLabel * dimLabel = new QLabel {QString{ "<font color='%3'>%1x%2</font>" }.arg(dimsA.width()).arg(dimsA.height()).arg(color), infoWidget};
	infoLayout->addWidget(dimLabel);
	
	switch (comp.date) {
		case CuttleCompInfo::status::high:
			color = "green";
			break;
		case CuttleCompInfo::status::same:
			color = "yellow";
			break;
		case CuttleCompInfo::status::low:
			color = "red";
			break;
	}
	
	QLabel * lastModifiedLabel = new QLabel {QString{ "<font color='%2'>%1</font>" }.arg(set->fi.lastModified().toString()).arg(color), infoWidget};
	infoLayout->addWidget(lastModifiedLabel);
	
	infoLayout->setMargin(0);
	infoLayout->setSpacing(20);
	layout->addWidget(infoWidget, 1, 0, 1, 1);
	
	QWidget * lowerWidget = new QWidget {this};
	QHBoxLayout * lowerLayout = new QHBoxLayout {lowerWidget};
	
	QPushButton * viewButton = new QPushButton {"VIEW"};
	viewButton->setMinimumWidth(15);
	viewButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(viewButton, &QPushButton::pressed, this, [=](){emit view(set);});
	lowerLayout->addWidget(viewButton);
	
	QPushButton * deleteButton = new QPushButton {"DELETE"};
	deleteButton->setMinimumWidth(15);
	deleteButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(deleteButton, &QPushButton::pressed, this, [=](){
		if (deleteButton->text() == "CONFIRM") {
			emit delete_me(set);
		} else {
			deleteButton->setText("CONFIRM");
			deleteButton->setStyleSheet("background-color: red");
		}
	});
	lowerLayout->addWidget(deleteButton);
	
	lowerLayout->setMargin(0);
	layout->addWidget(lowerWidget, 2, 0, 1, 1);
}
