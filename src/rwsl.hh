#pragma once

#include <atomic>

#define ASMPAUSE __asm volatile ("pause" ::: "memory")

struct rwslck final { // read/write spinlock
	
	rwslck() = default;
	
	inline void read_access() {
		while (accessor.test_and_set()) ASMPAUSE;
		while (write_sem.test_and_set()) ASMPAUSE;
		write_sem.clear();
		readers.fetch_add(1);
		accessor.clear();
	}
	
	inline bool read_access_try() {
		if (accessor.test_and_set()) return false;
		if (write_sem.test_and_set()) {
			accessor.clear();
			return false;
		}
		write_sem.clear();
		readers.fetch_add(1);
		accessor.clear();
		return true;
	}
	
	inline void read_done() {
		readers.fetch_add(-1);
	}
	
	inline void write_lock() {
		while (accessor.test_and_set()) ASMPAUSE;
		while (write_sem.test_and_set()) ASMPAUSE;
		while (readers.load()) ASMPAUSE;
		accessor.clear();
	}
	
	inline bool write_lock_try() {
		if (accessor.test_and_set()) return false;
		if (write_sem.test_and_set()) {
			accessor.clear();
			return false;
		}
		if (readers.load()) {
			write_sem.clear();
			accessor.clear();
			return false;
		}
		accessor.clear();
		return true;
	}
	
	inline void write_unlock() {
		write_sem.clear();
	}
	
	inline  void write_to_read() {
		readers.fetch_add(1);
		write_sem.clear();
	}
	
private:
	std::atomic_int_fast8_t readers {0};
	std::atomic_flag accessor {false};
	std::atomic_flag write_sem {false};
};
