#ifndef VNCD_SERVER_HH
#define VNCD_SERVER_HH

#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>
#include <unordered_map>

#include <unistdx/base/log_message>
#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/identity>
#include <unistdx/ipc/process_group>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

namespace vncd {

	class Connection;
	class Local_client;
	class Local_server;
	class Remote_client;
	class Server;

	class Connection {

	public:
		enum class State {
			Initial,
			Starting,
			Started,
			Stopping,
			Stopped,
		};

	private:
		Server* _parent = nullptr;
		State _state = State::Initial;

	protected:
		sys::socket _socket;

	public:

		Connection() = default;

		inline explicit
		Connection(sys::family_type family):
		_socket(family) {}

		virtual ~Connection() {}

		virtual void
		process(const sys::epoll_event& event) {
			if (initial()) {
				throw std::logic_error("bad state");
			}
			if (starting() && !event.bad()) {
				this->_state = State::Started;
			}
			if (started() && event.bad()) {
				this->stop();
			}
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

		inline sys::fd_type
		fd() const noexcept {
			return this->_socket.fd();
		}

		inline State
		state() const {
			return this->_state;
		}

		inline void
		state(State s) {
			this->_state = s;
		}

		inline bool
		initial() const {
			return this->_state == State::Initial;
		}

		inline bool
		starting() const {
			return this->_state == State::Starting;
		}

		inline bool
		started() const {
			return this->_state == State::Started;
		}

		inline bool
		stopping() const {
			return this->_state == State::Stopping;
		}

		inline bool
		stopped() const {
			return this->_state == State::Stopped;
		}

		inline void
		start() {
			if (this->_state != State::Initial) {
				throw std::logic_error("bad state");
			}
			this->_state = State::Starting;
		}

		inline void
		stop() {
			if (this->_state != State::Started) {
				throw std::logic_error("bad state");
			}
			this->_state = State::Stopping;
		}

	};

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
		return a.at() < b.at();
	}

	inline bool
	operator<(const std::unique_ptr<Task>& a, const std::unique_ptr<Task>& b) {
		return operator<(*a, *b);
	}

	class Server {

	private:
		friend class Local_server;

	private:
		typedef std::unique_ptr<Connection> connection_pointer;
		typedef sys::spin_mutex mutex_type;
		typedef sys::simple_lock<mutex_type> lock_type;
		typedef std::unique_ptr<Task> task_pointer;
		typedef Task::clock_type clock_type;
		typedef Task::time_point time_point;
		typedef Task::duration duration;

	private:
		sys::event_poller _poller;
		std::unordered_map<sys::fd_type,connection_pointer> _connections;
		sys::port_type _port;
		sys::port_type _vnc_base_port;
		mutex_type _mutex;
		std::priority_queue<task_pointer> _tasks;

	public:

		inline explicit
		Server(sys::port_type port, sys::port_type vnc_port):
		_port(port), _vnc_base_port(vnc_port) {}

		inline sys::port_type
		port() const noexcept {
			return this->_port;
		}

		inline sys::port_type
		vnc_base_port() const noexcept {
			return this->_vnc_base_port;
		}

		inline void
		add(Connection* connection) {
			if (!connection) {
				throw std::invalid_argument("bad connection");
			}
			connection->parent(this);
			auto fd = connection->fd();
			this->_connections.emplace(fd, connection);
			this->_poller.emplace(fd, sys::event::in);
			connection->start();
		}

		inline void
		add(Task* task) {
			if (!task) {
				throw std::invalid_argument("bad task");
			}
			task->parent(this);
			lock_type lock(this->_mutex);
			this->_tasks.emplace(task);
			this->_poller.notify_one();
		}

		void
		run() {
			lock_type lock(this->_mutex);
			while (true) {
				duration timeout = std::chrono::milliseconds(-1); // no timeout
				if (!this->_tasks.empty()) {
					auto dt = this->_tasks.top()->at() - clock_type::now();
					timeout = std::max(dt, duration::zero());
				}
				std::cv_status status = this->_poller.wait_for(lock, timeout);
				if (status == std::cv_status::timeout) {
					this->process_tasks();
				} else {
					this->process_events();
				}
			}
		}

	private:

		void
		process_events() {
			for (const auto& event : this->_poller) {
				auto result = this->_connections.find(event.fd());
				if (result == this->_connections.end()) {
					std::clog << "bad fd: " << event.fd() << std::endl;
					continue;
				}
				auto& connection = *result->second;
				connection.process(event);
				if (connection.stopped()) {
					this->_connections.erase(result);
				}
			}
		}

		void
		process_tasks() {
			auto now = clock_type::now();
			while (this->_tasks.top()->at() <= now) {
				task_pointer& tmp = const_cast<task_pointer&>(this->_tasks.top());
				task_pointer task = std::move(tmp);
				this->_tasks.pop();
				task->run();
				if (task->remaining_attempts() != 0 && task->has_period()) {
					task->at(now + task->period());
					this->_tasks.emplace(std::move(task));
				}
			}
		}

	};

	/// VNC client state.
	class Session {

	private:
		sys::uid_type _uid;
		sys::gid_type _gid;
		sys::socket _remote_socket;
		sys::socket _local_socket;
		sys::process_group _processes;
		sys::port_type _port;

	public:

		inline void
		set_port(sys::port_type p) {
			this->_port = p;
		}

		inline sys::port_type
		port() const {
			return this->_port;
		}

		inline void
		set_identity(sys::uid_type uid, sys::gid_type gid) {
			this->_uid = uid;
			this->_gid = gid;
		}

		inline void
		set_remote_socket(const sys::socket& s) {
			this->_remote_socket = s;
		}

		inline void
		set_local_socket(const sys::socket& s) {
			this->_local_socket = s;
		}

		void
		vnc_start() {
			try {
				this->_processes.emplace([this] () {this->vnc_main();});
			} catch (const std::exception& err) {
				sys::log_message(
					"failed to start VNC server for _: _",
					this->_uid,
					err.what()
				);
			}
		}

		void
		vnc_main() {
			sys::this_process::set_identity(this->_uid, this->_gid);
			const char* script = std::getenv("VNCD_SERVER");
			if (!script) {
				throw std::invalid_argument("VNCD_SERVER variable is not set");
			}
			sys::argstream args;
			args.append(script);
			::setenv("VNCD_UID", std::to_string(this->_uid).data(), 1);
			::setenv("VNCD_GID", std::to_string(this->_gid).data(), 1);
			::setenv("VNCD_PORT", std::to_string(this->_port).data(), 1);
			sys::this_process::execute(args);
		}

		void
		x_session_start() {
			try {
				this->_processes.emplace([this] () {this->x_session_main();});
			} catch (const std::exception& err) {
				sys::log_message(
					"failed to start X session for _: _",
					this->_uid,
					err.what()
				);
			}
		}

		void
		x_session_main() {
			sys::this_process::set_identity(this->_uid, this->_gid);
			const char* script = std::getenv("VNCD_SESSION");
			if (!script) {
				throw std::invalid_argument("VNCD_SESSION variable is not set");
			}
			sys::argstream args;
			args.append(script);
			sys::this_process::execute(args);
		}

	};

	/// Local VNC client that connects to the local VNC server.
	class Local_client: public Connection {

	private:
		std::shared_ptr<Session> _session;

	public:

		inline explicit
		Local_client(std::shared_ptr<Session> session):
		_session(session) {
			this->_socket.connect({{127,0,0,1},this->_session->port()});
		}

		void
		process(const sys::epoll_event& event) override {
			if (starting() && !event.bad()) {
				this->_session->set_local_socket(this->_socket);
				this->_session->x_session_start();
				this->state(State::Started);
			}
			if (started() && event.bad()) {
				this->stop();
			}
			if (started()) {
				if (event.in()) {
					// TODO splice
				}
			}
		}

	};

	class Local_client_task: public Task {

	private:
		std::shared_ptr<Session> _session;

	public:

		inline explicit
		Local_client_task(std::shared_ptr<Session> session):
		_session(session) {
			this->period(std::chrono::seconds(1));
			this->repeat(7);
		}

		void run() override {
			Task::run();
			this->parent().add(new Local_client(this->_session));
		}

	};

	/// VNC remote client that connects to one of the local servers.
	class Remote_client: public Connection {

	private:
		sys::socket_address _address;
		sys::process_group _vnc;
		std::shared_ptr<Session> _session{std::make_shared<Session>()};

	public:

		inline explicit
		Remote_client(sys::socket& server_socket, sys::uid_type uid) {
			this->_session->set_identity(uid, uid);
			this->_session->set_port(this->parent().vnc_base_port());
			server_socket.accept(this->_socket, this->_address);
		}

		void
		process(const sys::epoll_event& event) override {
			if (starting() && !event.bad()) {
				this->_session->set_remote_socket(this->_socket);
				this->_session->vnc_start();
				this->parent().add(new Local_client_task(this->_session));
				this->state(State::Started);
			}
			if (started() && event.bad()) {
				this->stop();
			}
			if (started()) {
				if (event.in()) {
					// TODO splice
				}
			}
		}

	};

	/// Local server that accepts connections on a particular port.
	class Local_server: public Connection {

	private:
		sys::socket_address _address;

	public:

		inline explicit
		Local_server(const sys::socket_address& address):
		Connection(address.family()), _address(address) {
			this->_socket.setopt(sys::socket::reuse_addr);
			this->_socket.bind(this->_address);
			this->_socket.listen();
		}

		inline sys::fd_type
		fd() const noexcept {
			return this->_socket.fd();
		}

		inline sys::port_type
		port() const noexcept {
			return this->_address.port();
		}

		void
		process(const sys::epoll_event& event) override {
			Connection::process(event);
			if (started() && event.in()) {
				sys::uid_type uid = this->parent().port() + this->port();
				std::clog << "user " << uid << " connected" << std::endl;
				this->parent().add(new Remote_client(this->_socket, uid));
			}
		}

	};

}

#endif // vim:filetype=cpp
