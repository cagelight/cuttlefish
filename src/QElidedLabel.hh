#pragma once

#include <QLabel>

class QElidedLabel : public QLabel {
	Q_OBJECT
	
public:
	QElidedLabel(QString const & text, QWidget * parent = nullptr);
	
	void setText(QString const & text);
	QString const & text();
	
protected:
	void updateElide();
	virtual void resizeEvent(QResizeEvent *) override;
	
private:
	QString m_source;
};
