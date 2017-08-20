#include <QApplication>
#include <QTextCodec>

#include "cuttleold.hh"

int main(int argc, char * * argv) {
	QApplication app {argc, argv};
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
	CuttleCore * cmw = new CuttleCore {};
	cmw->show();
	return app.exec();
}
