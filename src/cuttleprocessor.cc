#include "cuttle.hh"

#include <QDirIterator>
#include <QSet>
#include <QImageReader>

#include <chrono>
#include <mutex>
#include <ctgmath>

CuttleProcessor::CuttleProcessor(QObject * parent) : QObject(parent) {
	
}

CuttleProcessor::~CuttleProcessor() {
	if (worker) {
		worker_run.store(false);
		if (worker->joinable()) worker->join();
		delete worker;
	}
	if (match_data_size) {
		for (size_t i = 1; i < match_data_size; i++) {
			delete [] match_data_fast[i];
		}
		delete [] match_data_fast;
	}
}

void CuttleProcessor::beginProcessing(QList<CuttleDirectory> const & dirs) {
	
	qDebug() << "BEGIN PROCESSING";
	
	emit started();
	emit section("Preparing...");
	emit max(0);
	if (worker) {
		worker_run.store(false);
		if (worker->joinable()) worker->join();
		delete worker;
	}
	sets.clear();
	
	if (match_data_size) {
		for (size_t i = 1; i < match_data_size; i++) {
			delete [] match_data_fast[i];
		}
		delete [] match_data_fast;
	}
	
	worker_run.store(true);
	worker = new std::thread {[&](){
		
		std::atomic_uint_fast32_t group_id {0};
		if (dirs.size() > 1) group_id++;
		
		for (CuttleDirectory const & dir : dirs) {
			QDirIterator diter {dir.dir, QDir::Files, dir.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags};
			while (diter.hasNext()) {
				CuttleSet set {diter.next()};
				set.group = group_id;
				sets.push_back(std::move(set));
			}
			group_id++;
		}
		
		if (!this->worker_run) return;
		
		typedef std::vector<CuttleSet>::iterator cuiter;
		
		cuiter iter = sets.begin();
		std::vector<CuttleSet *> toRem {};
		rwslck sublk, emitlk;
		emit section("Loading images... %p%");
		emit value(0);
		emit max(sets.size());
		int img_i = 0;
		
		std::atomic_uint_fast32_t id {0};
		std::chrono::high_resolution_clock::time_point emit_limiter = std::chrono::high_resolution_clock::now();
		
		std::vector<std::thread *> subworkers;
		for (uint i = 0; i < std::thread::hardware_concurrency(); i++) subworkers.push_back(new std::thread([&](){
			while (this->worker_run) {
				sublk.write_lock();
				if (iter == sets.end()) {
					sublk.write_unlock();
					break;
				}
				cuiter set = iter++;
				
				if (emitlk.write_lock_try()) {
					std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
					if (now - emit_limiter > std::chrono::milliseconds(125)) {
						emit value(img_i);
						emit_limiter = now;
					}
					emitlk.write_unlock();
				}
				img_i++;
				
				sublk.write_unlock();
				CuttleSet & setref = const_cast<CuttleSet &>(*set); // FIXME -- Why do I have to do this? I'm not using a const_iterator yet it's only giving me const references...
				try {
					setref.generate(128);
					setref.id = id++;
				} catch (CuttleNullImageException) {
					setref.delete_me = true;
				}
			}
		}));
		for (std::thread * sw : subworkers) {
			if (sw->joinable()) sw->join();
			delete sw;
		}
		subworkers.clear();
		
		match_data_size = id;
		match_data_fast = new double * [match_data_size] {nullptr};
		for (uint_fast32_t x = 1; x < match_data_size; x++) {
			match_data_fast[x] = new double [x] {0};
		}
		
		emit max(1);
		emit value(1);
		
		sets.erase(std::remove_if(sets.begin(), sets.end(), [](CuttleSet & v){return v.delete_me;}), sets.end());
		
		int cmax = 0;
		for (uint i = 1; i <= sets.size(); i++) cmax += i;
		emit section("Generating deltas... %p%");
		emit value(0);
		emit max(cmax);
		img_i = 0;
		
		cuiter iterA = sets.begin();
		cuiter iterB = iterA;
		
		for (uint i = 0; i < std::thread::hardware_concurrency(); i++) subworkers.push_back(new std::thread([&](){
			while (this->worker_run) {
				cuiter curA, curB;
				sublk.write_lock();
				if (iterB == sets.end()) iterB = iterA++;
				if (iterA == sets.end()) {
					sublk.write_unlock();
					break;
				}
				curA = iterA;
				curB = iterB++;
				
				if (emitlk.write_lock_try()) {
					std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
					if (now - emit_limiter > std::chrono::milliseconds(125)) {
						emit value(img_i);
						emit_limiter = now;
					}
					emitlk.write_unlock();
				}
				img_i++;
				
				sublk.write_unlock();
				if (curA == curB) continue;
				if (curA->group && curB->group && curA->group == curB->group) continue;
				double val = CuttleSet::compare(curA.operator->(), curB.operator->());
				if (curA->id > curB->id) {
					match_data_fast[curA->id][curB->id] = val;
				} else {
					match_data_fast[curB->id][curA->id] = val;
				}
			}
		}));
		for (std::thread * sw : subworkers) {
			if (sw->joinable()) sw->join();
			delete sw;
		}
		
		if (worker_run) { // completed successfully
			emit section("Complete");
			emit value(1);
			emit max(1);
			emit finished();
		} else { // stopped
			sets.clear();
			emit section("Stopped");
			emit value(1);
			emit max(1);
			emit finished();
		}
	}};
}

double CuttleProcessor::getHigh(CuttleSet const * set) const {
	double high = 0;
	for (auto const & i : sets) {
		if (set->id == i.id) continue;
		double val = getMatchData(&i, set);
		if (val > high) high = val;
	}
	return high;
}

std::vector<CuttleSet const *> CuttleProcessor::getSetsAboveThresh(double high) const {
	std::vector<CuttleSet const *> vec {};
	for (auto const & i : sets) {
		if (getHigh(&i) >= high) vec.push_back(&i);
	}
	return vec;
}

std::vector<CuttleSet const *> CuttleProcessor::getSetsAboveThresh(CuttleSet const * comp, double thresh) const {
	std::vector<CuttleSet const *> vec {};
	for (auto const & i : sets) {
		if (comp->id == i.id) continue;
		if (getMatchData(&i, comp) >= thresh) vec.push_back(&i);
	}
	return vec;
}

void CuttleProcessor::remove(CuttleSet const * set) {
	emit started();
	sets.erase(std::remove_if(sets.begin(), sets.end(), [&set](CuttleSet & v){return v.id == set->id;}), sets.end());
	emit finished();
}

void CuttleProcessor::remove(CuttleSet const * setA, CuttleSet const * setB) {
	emit started();
	//sets.erase(std::remove_if(sets.begin(), sets.end(), [&](CuttleSet & v){return v.id == setA->id || v.id == setB->id;}), sets.end());
	if (setA->id > setB->id) match_data_fast[setA->id][setB->id] = 0;
	else match_data_fast[setB->id][setA->id] = 0;
	emit finished();
}

void CuttleProcessor::remove_all_idential() {
	emit started();
	auto iter = sets.begin();
	std::vector<std::vector<CuttleSet>::iterator> iter_set;
	while (iter != sets.end()) {
		for (auto const & iter2 : sets) {
			if (iter->id != iter2.id && getMatchData(*iter, iter2) == 1) {
				qDebug() << "MATCH";
			}
		}
		iter_set.clear();
		iter++;
	}
	emit finished();
}

QImage CuttleSet::getImage() const {
	QImageReader read {filename};
	read.setAutoDetectImageFormat(true);
	read.setDecideFormatFromContent(true);
	QImage img = read.read();
	*const_cast<QSize *>(&img_size) = img.size();
	return img;
}

void CuttleSet::generate(uint_fast16_t res) {
	
	if (this->res == res) return;
	this->res = res;
	
	QImage img = getImage();
	if (img.isNull()) {
		throw CuttleNullImageException {};
	}
	img = img.scaled({static_cast<int>(res), static_cast<int>(res)}, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	data.resize(res * res);
	
	int i = 0;
	for (uint_fast16_t y = 0; y < res; y++) {
		for (uint_fast16_t x = 0; x < res; x++) {
			data[i++] = img.pixelColor(x, y);
		}
	}
}

double CuttleSet::compare(CuttleSet const * A, CuttleSet const * B) {
	
	if (A == B) return 1;
	
	double match = 0;
	uint_fast16_t res = A->res;
	for (uint_fast16_t i = 0; i < res * res; i++) {
		
		QColor const & cA = A->data[i];
		QColor const & cB = B->data[i];
		
		double mR = abs(cA.redF() - cB.redF());
		double mG = abs(cA.greenF() - cB.greenF());
		double mB = abs(cA.blueF() - cB.blueF());
		double mL = mR;
		if (mL > mG) mL = mG;
		if (mL > mB) mL = mB;
		
		match += 1.0 - mL;
	}
	
	return match / (res * res);
}
