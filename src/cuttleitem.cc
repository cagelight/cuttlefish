#include "cuttle.hh"

#include "QElidedLabel.hh"
#include <QtWidgets>

CuttleLeftItem::CuttleLeftItem(QWidget * parent, CuttleSet const * set, CuttleProcessor * proc) : QFrame(parent), set(set), proc(proc) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	QGridLayout * layout = new QGridLayout {this};
	
	QFileInfo finfo { set->filename };
	QLabel * nameLabel = new QLabel { finfo.fileName(), this };
	nameLabel->setToolTip(finfo.canonicalFilePath());
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
		double v = proc->getMatchData(set, &cset).value;
		if (v > high) high = v;
	}
	
	QLabel * thumb = new QLabel {this};
	thumb->setFixedSize(THUMB_SIZE, THUMB_SIZE);
	thumb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	thumb->setPixmap(set->thumb);
	lowerLayout->addWidget(thumb);
	
	QPushButton * activateButton = new QPushButton {"GO"};
	token = activateButton;
	activateButton->setMinimumWidth(15);
	activateButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(activateButton, &QPushButton::clicked, this, [=](){emit activated(set);});
	lowerLayout->addWidget(activateButton);
	
	QLabel * highestMatchLabel = new QLabel {QString("High: %1").arg(high), lowerWidget};
	lowerLayout->addWidget(highestMatchLabel);
	
	lowerLayout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(lowerWidget, 1, 0, 1, 1);
	
	connect(this, &CuttleLeftItem::recalculateHigh, this, [=](){
		high = 0;
		for (CuttleSet const & cset : proc->getSets()) {
			if (cset.id == set->id) continue;
			double v = proc->getMatchData(set, &cset).value;
			if (v > high) high = v;
		}
	});
}

void CuttleLeftItem::activate() {
	reinterpret_cast<QPushButton *>(token)->click();
}

CuttleRightItem::CuttleRightItem(QWidget * parent, CuttleSet const * set, CuttleSet const * other, CuttleProcessor * proc) : QFrame(parent), set(set) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	QGridLayout * layout = new QGridLayout {this};
	
	QFileInfo finfo { set->filename };
	QLabel * nameLabel = new QLabel { finfo.fileName(), this };
	nameLabel->setToolTip(finfo.canonicalFilePath());
	nameLabel->setMinimumWidth(RIGHT_COLUMN_SIZE - 50);
	nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	QFontMetrics metrics { nameLabel->font() };
	layout->addWidget(nameLabel, 0, 0, 1, 1);
	nameLabel->setText( metrics.elidedText(nameLabel->text(), Qt::ElideMiddle, nameLabel->width()) );
	
	QWidget * lowerWidget = new QWidget {this};
	QHBoxLayout * lowerLayout = new QHBoxLayout {lowerWidget};
	
	value = proc->getMatchData(set, other).value;
	
	QLabel * thumb = new QLabel {this};
	thumb->setFixedSize(THUMB_SIZE, THUMB_SIZE);
	thumb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	thumb->setPixmap(set->thumb);
	lowerLayout->addWidget(thumb);
	
	QPushButton * activateButton = new QPushButton {"GO"};
	activateButton->setMinimumWidth(15);
	activateButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(activateButton, &QPushButton::clicked, this, [=](){emit activated(set);});
	lowerLayout->addWidget(activateButton);
	
	QLabel * highestMatchLabel = new QLabel {QString("Value: %1").arg(value), lowerWidget};
	lowerLayout->addWidget(highestMatchLabel);
	
	lowerLayout->setContentsMargins(0, 0, 0, 0);
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
	
	QFileInfo finfo { set->filename };
	QLineEdit * nameLabel = new QLineEdit { finfo.canonicalFilePath(), this };
	nameLabel->setReadOnly(true);
	nameLabel->setToolTip(finfo.canonicalFilePath());
	layout->addWidget(nameLabel, 0, 0, 1, 1);
	
	QWidget * infoWidget = new QWidget {this};
	QHBoxLayout * infoLayout = new QHBoxLayout {infoWidget};
	
	QLabel * eqLabel = new QLabel {QString{ "<font color='%2'>%1</font>" }.arg(comp.equal ? "EQUAL" : "NOT EQUAL").arg(comp.equal ? "green" : "yellow"), infoWidget};
	infoLayout->addWidget(eqLabel);
	
	char const * color;
	switch (comp.size) {
		case CuttleCompInfo::status::high:
			color = comp.equal ? "red" : "green";
			break;
		case CuttleCompInfo::status::same:
			color = comp.equal ? "green" : "yellow";
			break;
		case CuttleCompInfo::status::low:
			color = comp.equal ? "green" : "red";
			break;
	}
	
	QLabel * sizeLabel = new QLabel {QString{ "<font color='%2'>%1</font>" }.arg(readable_file_size(set->fi.size())).arg(color), infoWidget};
	infoLayout->addWidget(sizeLabel);
	
	if (comp.equal) color = "green";
	else switch (comp.dims) {
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
			color = comp.equal ? "green" : "yellow";
			break;
		case CuttleCompInfo::status::low:
			color = "red";
			break;
	}
	
	QLabel * lastModifiedLabel = new QLabel {QString{ "<font color='%2'>%1</font>" }.arg(set->fi.lastModified().toString()).arg(color), infoWidget};
	infoLayout->addWidget(lastModifiedLabel);
	
	infoLayout->setContentsMargins(0, 0, 0, 0);
	infoLayout->setSpacing(20);
	layout->addWidget(infoWidget, 1, 0, 1, 1);
	
	QWidget * lowerWidget = new QWidget {this};
	QHBoxLayout * lowerLayout = new QHBoxLayout {lowerWidget};
	
	QPushButton * viewButton = new QPushButton {"VIEW"};
	viewButton->setMinimumWidth(15);
	viewButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(viewButton, &QPushButton::clicked, this, [=](){emit view();});
	lowerLayout->addWidget(viewButton);
	
	QPushButton * deleteButton = new QPushButton {"DELETE"};
	deleteButton->setMinimumWidth(15);
	deleteButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
	connect(deleteButton, &QPushButton::clicked, this, [=](){
		if (deleteButton->text() == "CONFIRM") {
			emit delete_me(set);
		} else {
			deleteButton->setText("CONFIRM");
			deleteButton->setStyleSheet("background-color: red");
		}
	});
	lowerLayout->addWidget(deleteButton);
	
	lowerLayout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(lowerWidget, 2, 0, 1, 1);
}
