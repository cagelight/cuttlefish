#include "cuttle.hh"

#include "rw_spinlock.hh"

#include <QDirIterator>
#include <QSet>
#include <QImageReader>
#include <QCryptographicHash>

#include <chrono>
#include <mutex>
#include <ctgmath>

CuttleProcessor::CuttleProcessor(QObject * parent) : QObject(parent) {}

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

void CuttleProcessor::beginProcessing(QList<CuttleDirectory> const & dirs, size_t res) {
	
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
	worker = new std::thread {[&, res](){
		
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
		rw_spinlock sublk, emitlk;
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
					setref.generate(res);
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
		match_data_fast = new CuttleMatchData * [match_data_size] {nullptr};
		for (uint_fast32_t x = 1; x < match_data_size; x++) {
			match_data_fast[x] = new CuttleMatchData [x] {};
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
				if (curA->group && curB->group && curA->group == curB->group)
					continue;
				
				CuttleMatchData val = CuttleSet::compare(curA.base(), curB.base());
				
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
		double val = getMatchData(&i, set).value;
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
		if (getMatchData(&i, comp).value >= thresh) vec.push_back(&i);
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
	if (setA->id > setB->id) match_data_fast[setA->id][setB->id] = invalid_match;
	else match_data_fast[setB->id][setA->id] = invalid_match;
	emit finished();
}

void CuttleProcessor::remove_all_idential() {
	emit started();
	auto iter = sets.begin();
	std::vector<std::vector<CuttleSet>::iterator> iter_set;
	while (iter != sets.end()) {
		for (auto const & iter2 : sets) {
			if (iter->id != iter2.id && getMatchData(*iter, iter2).value == 1) {
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
	read.setAllocationLimit(4096);
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
	thumb = QPixmap::fromImage(img.scaled(THUMB_SIZE, THUMB_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	img = img.scaled({static_cast<int>(res), static_cast<int>(res)}, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	data.resize(res * res);
	
	QCryptographicHash hash {QCryptographicHash::Sha512};
	hash.addData(reinterpret_cast<char const *>(img.constBits()), img.sizeInBytes());
	img_hash = hash.result();
	
	int i = 0;
	for (uint_fast16_t y = 0; y < res; y++) {
		for (uint_fast16_t x = 0; x < res; x++) {
			data[i++] = img.pixelColor(x, y);
		}
	}
	
	// CV
	
	QImage testI = img.convertedTo(QImage::Format_RGB32);
	std::vector<uint8_t> test;
	test.reserve(testI.width() * testI.height() * 3);
	for (int x = 0; x < res; x++) for (int y = 0; y < res; y++) {
		QRgb c = testI.pixel(x, y);
		test.push_back( qRed(c) );
		test.push_back( qGreen(c) );
		test.push_back( qBlue(c) );
	}
	
	cv::Mat cvm = cv::Mat(res, res, CV_8UC3, (void *)test.data());
	std::vector<cv::Mat> bgr_planes;
    cv::split( cvm, bgr_planes );
	
	int histSize = 256;
    float range[] = { 0, 256 }; //the upper boundary is exclusive
    const float* histRange[] = { range };
	
	calcHist(&bgr_planes[0], 1, 0, cv::Mat(), b_hist, 1, &histSize, histRange, true, false);
	calcHist(&bgr_planes[1], 1, 0, cv::Mat(), g_hist, 1, &histSize, histRange, true, false);
	calcHist(&bgr_planes[2], 1, 0, cv::Mat(), r_hist, 1, &histSize, histRange, true, false);
	
	normalize(b_hist, b_hist, 1.0, 0.0, cv::NORM_L1);
	normalize(g_hist, g_hist, 1.0, 0.0, cv::NORM_L1);
	normalize(r_hist, r_hist, 1.0, 0.0, cv::NORM_L1);
}

CuttleMatchData CuttleSet::compare(CuttleSet const * A, CuttleSet const * B) {
	
	if (A == B) return perfect_match;
	if (A->img_hash == B->img_hash && A->getImage() == B->getImage()) return perfect_match;
	
	CuttleMatchData dat;
	dat.value = 0;
	dat.value += 0.3 * compare_pix(A, B);
	dat.value += 0.7 * compare_hist(A, B);
	
	return dat;
}

double CuttleSet::compare_pix(CuttleSet const * A, CuttleSet const * B) {
	double match = 0;
	uint_fast16_t res = A->res;
	
	for (uint_fast16_t i = 0; i < res * res; i++) {
		
		QColor const & cA = A->data[i];
		QColor const & cB = B->data[i];
		
		double mR = abs(cA.redF() - cB.redF());
		double mG = abs(cA.greenF() - cB.greenF());
		double mB = abs(cA.blueF() - cB.blueF());
		
		match += (3.0 - (mR + mG + mB)) / 3.0;
	}
	
	return match / (res * res);
}

double CuttleSet::compare_hist(CuttleSet const * A, CuttleSet const * B) {
	double rH = compareHist(A->r_hist, B->r_hist, cv::HISTCMP_BHATTACHARYYA);
	double gH = compareHist(A->g_hist, B->g_hist, cv::HISTCMP_BHATTACHARYYA);
	double bH = compareHist(A->b_hist, B->b_hist, cv::HISTCMP_BHATTACHARYYA);
	double sH = std::max({rH, gH, bH});
	
	return 1 - sH;
}
