#include "cuttle.hh"

#include <QtWidgets>

CuttleBuilderDirEntry::CuttleBuilderDirEntry(CuttleDirectory & dir, QWidget * parent) : QFrame(parent), dir(dir) {
	setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	QGridLayout * layout = new QGridLayout {this};
	
	QLabel * label = new QLabel {dir.dir, this};
	label->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	layout->addWidget(label, 0, 0, 1, 1);
	
	QWidget * controlWidget = new QWidget {this};
	controlWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	QHBoxLayout * controlLayout = new QHBoxLayout {controlWidget};
	controlLayout->setMargin(0);
	QLabel * recurseLabel = new QLabel {"Recurse:", controlWidget};
	controlLayout->addWidget(recurseLabel);
	QCheckBox * recurseCB = new QCheckBox {controlWidget};
	recurseCB->setChecked(dir.recursive);
	controlLayout->addWidget(recurseCB);
	QPushButton * deleteThis = new QPushButton {"Remove", this};
	controlLayout->addWidget(deleteThis);
	layout->addWidget(controlWidget, 1, 0, 1, 1);
	
	connect(deleteThis, &QPushButton::pressed, [this](){emit remove(this);});
	connect(recurseCB, &QCheckBox::stateChanged, [this](int state){this->dir.recursive = (state == Qt::Checked);});
}

CuttleBuilder::CuttleBuilder(QWidget * parent) : QWidget {parent, Qt::Window} {
	QGridLayout * layout = new QGridLayout {this};
	
	QScrollArea * dirListArea = new QScrollArea {this};
	dirListArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	dirListArea->setWidgetResizable(true);
	dirListWidget = new QWidget {dirListArea};
	dirListArea->setMinimumSize(300, 400);
	dirListArea->setWidget(dirListWidget);
	new QVBoxLayout {dirListWidget};
	layout->addWidget(dirListArea, 0, 0, 1, 1);
	
	QPushButton * addBut = new QPushButton {"Add", this};
	layout->addWidget(addBut, 1, 0, 1, 1);
	
	QPushButton * goBut = new QPushButton {"Go", this};
	layout->addWidget(goBut, 2, 0, 1, 1);
	
	connect(addBut, &QPushButton::pressed, [this](){
		QString dir = QFileDialog::getExistingDirectory(this, "Select Directory", QDir::currentPath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (dir.isNull()) return;
		CuttleDirectory cdir {dir, false};
		dirs.append(cdir);
		buildView();
	});
	connect(goBut, &QPushButton::pressed, [this](){emit begin(dirs); hide();});
}

void CuttleBuilder::focus() {
	hide();
	show();
	setFocus();
}

void CuttleBuilder::buildView() {
	for (CuttleBuilderDirEntry * entry : entries) delete entry;
	entries.clear();
	for (CuttleDirectory & dir : dirs) {
		CuttleBuilderDirEntry * entry = new CuttleBuilderDirEntry {dir, dirListWidget};
		entries.append(entry);
		dirListWidget->layout()->addWidget(entry);
		connect(entry, &CuttleBuilderDirEntry::remove, [this](CuttleBuilderDirEntry * e) {
			entries.removeAll(e);
			dirs.removeAll(e->getDir());
			delete e;
		});
	}
}
