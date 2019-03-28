#pragma once

#include <QMainWindow>
#include <QDir>
#include <QList>
#include <QFrame>
#include <QDebug>

#include <atomic>
#include <thread>
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

struct CuttleMatchData {
	double value;
	bool identical;
};

static constexpr CuttleMatchData perfect_match { 1.0, true };
static constexpr CuttleMatchData invalid_match { 0.0, false };

//--------------------------------

struct CuttleSet {
	CuttleSet(QString const & filename) : filename(filename), fi(filename) {}
	QImage getImage() const;
	void generate(uint_fast16_t res);
	QString filename;
	uint_fast32_t group = 0;
	uint_fast32_t id = 0;
	uint_fast16_t res = 0;
	bool delete_me = false;
	std::vector<QColor> data {};
	QFileInfo fi;
	QSize img_size {0, 0};
	QByteArray img_hash;
	inline QSize get_size() const {
		if (img_size == QSize {0, 0}) getImage();
		return img_size;
	}
	static CuttleMatchData compare(CuttleSet const * A, CuttleSet const * B);
};

//--------------------------------

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
	void remove(CuttleSet const * setA, CuttleSet const * setB);
	void remove_all_idential();
	inline CuttleMatchData const & getMatchData(CuttleSet const * A, CuttleSet const * B) const {
		if (A->group && B->group && A->group == B->group) return invalid_match;
		if (A->id == B->id) return perfect_match;
		if (A->id > B->id) return match_data_fast[A->id][B->id];
		else return match_data_fast[B->id][A->id];
	}
	inline CuttleMatchData const & getMatchData(CuttleSet const & A, CuttleSet const & B) const {
		if (A.group && B.group && A.group == B.group) return invalid_match;
		if (A.id == B.id) return perfect_match;
		if (A.id > B.id) return match_data_fast[A.id][B.id];
		else return match_data_fast[B.id][A.id];
	}
protected:
	std::vector<CuttleSet> sets {};
	uint_fast32_t match_data_size = 0;
	CuttleMatchData * * match_data_fast = nullptr;
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
	CuttleSet const * set;
	CuttleLeftItem(QWidget * parent, CuttleSet const *, CuttleProcessor *);
	inline double getHigh() const { return high; }
private:
	CuttleProcessor * proc = nullptr;
	double high;
signals:
	void activated(CuttleSet const * _this);
	void recalculateHigh();
};

class CuttleRightItem : public QFrame {
	Q_OBJECT
public:
	CuttleSet const * set;
	CuttleRightItem(QWidget * parent, CuttleSet const * set, CuttleSet const * other, CuttleProcessor * proc);
	inline double getValue() const { return value; }
private:
	double value;
signals:
	void activated(CuttleSet const * _this);
};

struct CuttleCompInfo {
	enum struct status {
		high,
		same,
		low
	};
	bool equal;
	status size;
	status dims;
	status date;
	static void GetCompInfo(CuttleSet const * A, CuttleSet const * B, CuttleCompInfo & Ac, CuttleCompInfo & Ab);
};

class CuttleCompItem : public QFrame {
	Q_OBJECT
public:
	CuttleSet const * set;
	CuttleCompItem(QWidget * parent, CuttleSet const * set, CuttleCompInfo const & info, CuttleProcessor * proc);
signals:
	void view();
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
