#include "QElidedLabel.hh"

QElidedLabel::QElidedLabel(QString const & text, QWidget * parent) : QLabel { parent }, m_source { text } {
	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
	updateElide();
}

void QElidedLabel::setText(QString const & text) {
	m_source = text;
	updateElide();
}

QString const & QElidedLabel::text() {
	return m_source;
}

void QElidedLabel::updateElide() {
	QFontMetrics metrics { font() };
	QLabel::setText( metrics.elidedText(m_source, Qt::ElideMiddle, width()) );
	setMinimumWidth(0);
}

void QElidedLabel::resizeEvent(QResizeEvent * QRE) {
	QLabel::resizeEvent(QRE);
	updateElide();
}
