// SPDX-License-Identifier: gpl3+

#ifndef VNCD_TASK_HH
#define VNCD_TASK_HH

#include <chrono>
#include <memory>

namespace vncd {

	class Server;

	class Task {

	public:
		typedef std::chrono::system_clock clock_type;
		typedef clock_type::time_point time_point;
		typedef clock_type::duration duration;

	private:
		Server* _parent = nullptr;
		time_point _at{duration::zero()};
		duration _period{duration::zero()};
		int _nattempts = 1;

	public:

		Task() = default;

		virtual
		~Task() = default;

		Task(const Task&) = default;

		Task(Task&&) = default;

		Task&
		operator=(Task&&) = default;

		Task&
		operator=(const Task&) = default;

		virtual void
		run() {
			if (this->_nattempts > 0) {
				--this->_nattempts;
			}
		}

		inline void
		at(time_point rhs) {
			this->_at = rhs;
		}

		inline time_point
		at() const {
			return this->_at;
		}

		inline duration
		period() const {
			return this->_period;
		}

		inline void
		period(duration d) {
			this->_period = d;
		}

		inline bool
		has_period() const {
			return this->_period != duration::zero();
		}

		inline void
		repeat(int n) {
			this->_nattempts = n;
		}

		inline void
		repeat_forever() {
			this->repeat(-1);
		}

		inline int
		remaining_attempts() const {
			return this->_nattempts;
		}

		inline void
		parent(Server* rhs) {
			this->_parent = rhs;
		}

		inline Server&
		parent() {
			return *this->_parent;
		}

		inline const Server&
		parent() const {
			return *this->_parent;
		}

	};

	inline bool
	operator<(const Task& a, const Task& b) {
		return a.at() > b.at();
	}

	inline bool
	operator<(const std::unique_ptr<Task>& a, const std::unique_ptr<Task>& b) {
		return operator<(*a, *b);
	}

}

#endif // vim:filetype=cpp
