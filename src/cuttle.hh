#pragma once

#include <QMainWindow>
#include <QDir>
#include <QList>
#include <QFrame>
#include <QDebug>

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "imgview.hh"

#define LEFT_COLUMN_SIZE 280
#define RIGHT_COLUMN_SIZE 280

//================================
//--------------------------------
//================================

struct CuttleDirectory {
	QString dir;
	bool recursive;
	bool operator == (CuttleDirectory const & other) const {
		return (&other == this) || (dir == other.dir && recursive == other.recursive);
	}
};

struct CuttleNullImageException { };

//================================
//--------------------------------
//================================

class CuttleBuilderDirEntry : public QFrame {
	Q_OBJECT
public:
	CuttleBuilderDirEntry(CuttleDirectory & dir, QWidget * parent);
	inline CuttleDirectory const & getDir() const {return dir;}
private:
	CuttleDirectory & dir;
signals:
	void remove(CuttleBuilderDirEntry *);
};

//--------------------------------

class CuttleBuilder : public QWidget {
	Q_OBJECT
public:
	CuttleBuilder(QWidget * parent);
	void focus();
signals:
	void begin(QList<CuttleDirectory> const &);
protected:
	void buildView();
	QList<CuttleDirectory> dirs;
	QList<CuttleBuilderDirEntry *> entries;
	QWidget * dirListWidget = nullptr;
};

//================================
//--------------------------------
//================================

struct CuttleSet {
	CuttleSet(QString const & filename) : filename(filename), fi(filename) {}
	QImage getImage() const;
	void generate(uint_fast16_t res);
	QString filename;
	uint_fast32_t id = 0;
	uint_fast16_t res = 0;
	bool delete_me = false;
	std::vector<QColor> data {};
	QFileInfo fi;
	QSize img_size {0, 0};
	inline QSize get_size() const {
		if (img_size == QSize {0, 0}) getImage();
		return img_size;
	}
	bool operator == (CuttleSet const & other) const {
		if (id && other.id) return id == other.id;
		else return (filename == other.filename);
	}
	struct Hash {
		std::size_t operator()(CuttleSet const & p) const {
			if (p.id) return p.id;
			else return qHash(p.filename);
		}
	};
	static double compare(CuttleSet const & A, CuttleSet const & B);
};

class CuttleProcessor : public QObject {
	Q_OBJECT
public:
	CuttleProcessor(QObject * parent);
	~CuttleProcessor();
	void beginProcessing(QList<CuttleDirectory> const & dirs);
	inline void stop() {worker_run.store(false);}
	inline std::vector<CuttleSet> const & getSets() const { return sets; }
	double getHigh(CuttleSet const * set) const;
	std::vector<CuttleSet const *> getSetsAboveThresh(double high) const;
	std::vector<CuttleSet const *> getSetsAboveThresh(CuttleSet const * comp, double thresh) const;
	void remove(CuttleSet const * set);
	inline double getMatchData(CuttleSet const & A, CuttleSet const & B) const {
		if (A.id == B.id) return 1;
		if (A.id > B.id) return match_data_fast[A.id][B.id];
		else return match_data_fast[B.id][A.id];
	}
protected:
	struct CuttlePair {
		uint_fast32_t A;
		uint_fast32_t B;
		bool operator == (CuttlePair const & other) const {
			return (A == other.A && B == other.B) || (A == other.B && B == other.A);
		}
		struct Hash {
			std::size_t operator()(CuttlePair const & p) const {
				return std::hash<uint_fast32_t>{}(p.A) + std::hash<uint_fast32_t>{}(p.B);
			}
		};
	};
	std::vector<CuttleSet> sets {};
	uint_fast32_t match_data_size = 0;
	double * * match_data_fast = nullptr;
private:
	std::atomic_bool worker_run {false};
	std::thread * worker = nullptr;
signals:
	void started();
	//--- PROGRESS BAR STUFF
	void section(QString);
	void max(int);
	void value(int);
	//---
	void finished();
};

//================================
//--------------------------------
//================================

class CuttleLeftItem : public QFrame {
	Q_OBJECT
public:
	CuttleLeftItem(QWidget * parent, CuttleSet const &, CuttleProcessor *);
	inline double getHigh() const { return high; }
private:
	CuttleProcessor * proc = nullptr;
	double high;
signals:
	void activated(CuttleSet const * _this);
};

class CuttleRightItem : public QFrame {
	Q_OBJECT
public:
	CuttleRightItem(QWidget * parent, CuttleSet const & set, CuttleSet const & other, CuttleProcessor * proc);
	inline double getValue() const { return value; }
	CuttleSet const * set;
private:
	double value;
signals:
	void activated(CuttleSet const * _this);
};

class CuttleCompItem : public QFrame {
	Q_OBJECT
public:
	CuttleCompItem(QWidget * parent, CuttleSet const & set, CuttleSet const & other, CuttleProcessor * proc);
signals:
	void view(CuttleSet const * _this);
	void delete_me(CuttleSet const * _this);
};

//--------------------------------

class CuttleCore : public QMainWindow {
	Q_OBJECT
public: 
	CuttleCore();
	virtual ~CuttleCore() = default;
protected:
	void rebuild();
	void generateRight(CuttleSet const * set);
private:
	CuttleBuilder * builder = nullptr;
	CuttleProcessor * processor = nullptr;
	ImageView * view = nullptr;
	QList<CuttleLeftItem *> leftList {};
	QList<CuttleRightItem *> rightList {};
	QList<CuttleCompItem *> compList {};
};

//================================
//--------------------------------
//================================
