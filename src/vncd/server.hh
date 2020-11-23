// SPDX-License-Identifier: gpl3+

#ifndef VNCD_SERVER_HH
#define VNCD_SERVER_HH

#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <unistdx/base/log_message>
#include <unistdx/base/simple_lock>
#include <unistdx/base/spin_mutex>
#include <unistdx/io/poller>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/identity>
#include <unistdx/ipc/process_group>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <vncd/task.hh>
#include <vncd/user.hh>

namespace vncd {

	class Connection;
	class Local_client;
	class Local_server;
	class Remote_client;
	class Server;

	struct No_lock {
		No_lock() {}
		template <class T> No_lock(T&) {}
		void lock() {}
		void unlock() {}
	};

	template <class T>
	inline void
	environment(const char* key, const T& value) {
		std::stringstream str;
		str << value;
		UNISTDX_CHECK(::setenv(key, str.str().data(), 1));
	}

	inline void
	environment(const char* key, const std::string& value) {
		std::stringstream str;
		str << value;
		UNISTDX_CHECK(::setenv(key, value.data(), 1));
	}

	class Connection {

	public:
		typedef Task::clock_type clock_type;
		typedef Task::time_point time_point;
		typedef Task::duration duration;

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
			if (initial()) { throw std::logic_error("bad state"); }
			if (starting() && !event.bad()) { this->_state = State::Started; }
			if (started() && event.bad()) { this->state(State::Stopping); }
			if (stopping()) { this->state(State::Stopped); }
		}

		inline void
		set_user_timeout(const duration& d) {
			this->_socket.set_user_timeout(d);
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

		inline sys::port_type
		port() const {
			return sys::socket_address_cast<sys::ipv4_socket_address>(this->_socket.bind_addr()).port();
		}

	};

	class Server {

	private:
		typedef std::unique_ptr<Connection> connection_pointer;
		typedef sys::spin_mutex mutex_type;
		typedef No_lock lock_type;
		typedef std::unique_ptr<Task> task_pointer;
		typedef Task::clock_type clock_type;
		typedef Task::time_point time_point;
		typedef Task::duration duration;

	private:
		sys::event_poller _poller;
		std::unordered_map<sys::fd_type,connection_pointer> _connections;
		std::priority_queue<task_pointer> _tasks;
		duration _timeout = duration::zero();
		mutex_type _mutex;

	public:

		inline void
		set_user_timeout(const duration& d) {
			this->_timeout = d;
		}

		inline void
		add(Connection* connection, sys::event events=sys::event::in) {
			if (!connection) {
				throw std::invalid_argument("bad connection");
			}
			connection->parent(this);
			connection->set_user_timeout(this->_timeout);
			auto fd = connection->fd();
			lock_type lock(this->_mutex);
			this->_connections.emplace(fd, connection_pointer(connection));
			this->_poller.emplace(fd, events);
			connection->start();
		}

		inline void
		remove(sys::port_type port) {
			lock_type lock(this->_mutex);
			auto first = this->_connections.begin();
			auto last = this->_connections.end();
			while (first != last) {
				auto& connection = *first->second;
				if (connection.port() == port) {
					first = this->_connections.erase(first);
					last = this->_connections.end();
				} else {
					++first;
				}
			}
		}

		template <class T>
		inline void
		submit(std::unique_ptr<T>&& task) {
			static_assert(std::is_base_of<Task,T>::value, "bad type");
			this->submit(task.release());
		}

		inline void
		submit(Task* task) {
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
					using namespace std::chrono;
					auto s = duration_cast<seconds>(dt);
				}
				std::cv_status status;
				bool success = false;
				while (!success) {
					try {
						status = this->_poller.wait_for(lock, timeout);
						success = true;
					} catch (const sys::bad_call& err) {
						if (err.errc() != std::errc::interrupted) {
							throw;
						}
					}
				}
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
			auto pipe_fd = this->_poller.pipe_in();
			for (const auto& event : this->_poller) {
				if (event.fd() == pipe_fd) {
					continue;
				}
				auto result = this->_connections.find(event.fd());
				if (result == this->_connections.end()) {
					this->log("bad fd _", event.fd());
					continue;
				}
				auto& connection = *result->second;
				try {
					connection.process(event);
				} catch (const std::exception& err) {
					this->log("session error: _", err.what());
				}
				if (connection.stopped()) {
					this->_connections.erase(result);
				}
			}
		}

		void
		process_tasks() {
			auto now = clock_type::now();
			while (!this->_tasks.empty() && this->_tasks.top()->at() <= now) {
				task_pointer& tmp = const_cast<task_pointer&>(this->_tasks.top());
				task_pointer task = std::move(tmp);
				this->_tasks.pop();
				try {
					task->run();
				} catch (const std::exception& err) {
					this->log("task error: _", err.what());
				}
				if (task->remaining_attempts() != 0 && task->has_period()) {
					task->at(now + task->period());
					this->_tasks.emplace(std::move(task));
				}
			}
		}

		template <class ... Args>
		inline void
		log(const char* message, const Args& ... args) const {
			sys::log_message("server", message, args...);
		}

	};

	/// VNC client state.
	class Session {

	private:
		User _user;
		sys::socket _remote_socket;
		sys::socket _local_socket;
		sys::process_group _processes;
		sys::port_type _port;
		sys::port_type _vnc_port;
		sys::pipe _in;
		sys::pipe _out;
		size_t _buffer_size = 65536;
		sys::splice _splice;
		bool _terminated = false;
		bool _verbose = false;

	public:

		inline explicit
		Session(const User& user):
		_user(user) {
			this->_buffer_size = this->_in.in().pipe_buffer_size();
		}

		inline void
		verbose(bool b) {
			this->_verbose = b;
		}

		inline void
		set_vnc_port(sys::port_type p) {
			this->_vnc_port = p;
		}

		inline void
		set_port(sys::port_type p) {
			this->_port = p;
		}

		inline sys::port_type
		port() const {
			return this->_port;
		}

		inline sys::port_type
		vnc_port() const {
			return this->_vnc_port;
		}

		inline void
		set_identity() {
			if (sys::this_process::user() == this->_user.id() &&
				sys::this_process::group() == this->_user.group_id()) {
				return;
			}
			sys::this_process::workdir("/");
			UNISTDX_CHECK(::initgroups(
				this->_user.name().data(),
				this->_user.group_id()
			));
			sys::this_process::set_identity(
				this->_user.id(),
				this->_user.group_id()
			);
			sys::this_process::workdir(this->_user.home().data());
			environment("HOME", this->_user.home());
			environment("SHELL", this->_user.shell());
			environment("USER", this->_user.name());
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
				this->log("failed to start VNC server: _", err.what());
			}
		}

		void
		vnc_main() {
			this->set_identity();
			const char* script = std::getenv("VNCD_SERVER");
			if (!script) {
				throw std::invalid_argument("VNCD_SERVER variable is not set");
			}
			sys::argstream args;
			args.append(script);
			this->log("executing _", args);
			environment("VNCD_UID", this->_user.id());
			environment("VNCD_GID", this->_user.group_id());
			environment("VNCD_PORT", vnc_port());
			sys::this_process::execute(args);
		}

		void
		x_session_start() {
			try {
				this->_processes.emplace([this] () {this->x_session_main();});
			} catch (const std::exception& err) {
				this->log("failed to start X session: _", err.what());
			}
		}

		void
		x_session_main() {
			this->set_identity();
			const char* script = std::getenv("VNCD_SESSION");
			if (!script) {
				throw std::invalid_argument("VNCD_SESSION variable is not set");
			}
			sys::argstream args;
			args.append(script);
			this->log("executing _", args);
			environment("DISPLAY", ':' + std::to_string(this->_user.id()));
			sys::this_process::execute(args);
		}

		void
		copy_from_remote_to_pipe() {
			if (!this->_remote_socket) {
				return;
			}
			ssize_t n = 0;
			do {
				n = this->_splice(
					this->_remote_socket,
					this->_in,
					this->_buffer_size
				);
			} while (n > 0);
			if (this->_verbose) {
				this->log("_ _", __func__, n);
			}
		}

		void
		copy_from_pipe_to_local() {
			if (!this->_local_socket) {
				return;
			}
			ssize_t n = 0;
			do {
				n = this->_splice(
					this->_in,
					this->_local_socket,
					this->_buffer_size
				);
			} while (n > 0);
			if (this->_verbose) {
				this->log("_ _", __func__, n);
			}
		}

		void
		copy_from_local_to_pipe() {
			if (!this->_local_socket) {
				return;
			}
			ssize_t n = 0;
			do {
				n = this->_splice(
					this->_local_socket,
					this->_out,
					this->_buffer_size
				);
			} while (n > 0);
			if (this->_verbose) {
				this->log("_ _", __func__, n);
			}
		}

		void
		copy_from_pipe_to_remote() {
			if (!this->_remote_socket) {
				return;
			}
			ssize_t n = 0;
			do {
				n = this->_splice(
					this->_out,
					this->_remote_socket,
					this->_buffer_size
				);
			} while (n > 0);
			if (this->_verbose) {
				this->log("_ _", __func__, n);
			}
		}

		void
		flush() {
			copy_from_local_to_pipe();
			copy_from_pipe_to_local();
			copy_from_remote_to_pipe();
			copy_from_pipe_to_remote();
		}

		inline bool
		has_been_terminated() const {
			return _terminated;
		}

		void
		terminate() {
			if (has_been_terminated()) {
				return;
			}
			this->log("terminate");
			try {
				this->_processes.terminate();
			} catch (const sys::bad_call& err) {
				if (err.errc() != std::errc::no_such_process) {
					throw;
				}
			}
			try {
				No_lock lock;
				this->_processes.wait(
					lock,
					[this](const sys::process&, const sys::process_status& status) {
						this->log("process exited with status _", status);
					}
				);
			} catch (const sys::bad_call& err) {
				if (err.errc() != std::errc::no_child_process) {
					throw;
				}
			}
			this->_in.close();
			this->_out.close();
			this->_local_socket.close();
			this->_remote_socket.close();
			this->_terminated = true;
		}

		template <class ... Args>
		inline void
		log(const char* message, const Args& ... args) const {
			sys::log_message(this->_user.name().data(), message, args...);
		}

	};

	typedef std::shared_ptr<Session> session_pointer;

	/// Local VNC client that connects to the local VNC server.
	class Local_client: public Connection {

	private:
		std::shared_ptr<Session> _session;

	public:

		inline explicit
		Local_client(std::shared_ptr<Session> session):
		_session(session) {
			sys::ipv4_socket_address address{{127,0,0,1},this->_session->vnc_port()};
			session->log("connecting to _", address);
			this->_socket.bind(sys::ipv4_socket_address{{127,0,0,1},0});
			this->_socket.connect(address);
		}

		void
		process(const sys::epoll_event& event) override {
			if (starting() && !event.bad()) {
				this->_session->set_local_socket(this->_socket);
				this->_session->flush();
				this->_session->x_session_start();
				this->state(State::Started);
			}
			if (started() && event.bad()) {
				this->_session->terminate();
				this->state(State::Stopping);
			}
			if (started()) {
				if (event.in()) {
					this->_session->copy_from_local_to_pipe();
					this->_session->copy_from_pipe_to_remote();
				}
				if (event.out()) {
					this->_session->copy_from_pipe_to_local();
					this->_session->copy_from_remote_to_pipe();
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
			this->repeat(1);
			this->at(clock_type::now() + this->period());
		}

		void run() override {
			Task::run();
			this->_session->log("attempts left _", remaining_attempts());
			this->parent().add(
				new Local_client(this->_session),
				sys::event::inout
			);
		}

	};

	/// VNC remote client that connects to one of the local servers.
	class Remote_client: public Connection {

	private:
		sys::socket_address _address;
		sys::process_group _vnc;
		std::shared_ptr<Session> _session;

	public:

		inline explicit
		Remote_client(sys::socket& server_socket,
                      session_pointer session,
                      sys::socket&& socket,
                      const sys::socket_address& address):
        _address(address),
		_session(std::move(session)) {
			this->_session->set_remote_socket(this->_socket);
			this->_session->vnc_start();
		}

		void
		process(const sys::epoll_event& event) override {
			if (starting() && !event.bad()) {
				this->session()->log("accept");
				this->state(State::Started);
			}
			if (started() && event.bad()) {
				this->_session->terminate();
				this->state(State::Stopping);
			}
			if (started()) {
				if (event.in()) {
					this->_session->copy_from_remote_to_pipe();
					this->_session->copy_from_pipe_to_local();
				}
				if (event.out()) {
					this->_session->copy_from_pipe_to_remote();
					this->_session->copy_from_local_to_pipe();
				}
			}
		}

		inline const session_pointer&
		session() const {
			return this->_session;
		}

	};

	/// Local server that accepts connections on a particular port.
	class Local_server: public Connection {

	private:
		sys::socket_address _address;
		sys::port_type _vnc_port;
		User _user;
		bool _verbose;
		session_pointer _session;

	public:

		inline explicit
		Local_server(
			const sys::socket_address& address,
			sys::port_type vnc_port,
			const User& user,
			bool verbose
		):
		Connection(address.family()),
		_address(address),
		_vnc_port(vnc_port),
		_user(user),
		_verbose(verbose) {
			this->_socket.set(sys::socket::options::reuse_address);
			this->_socket.bind(this->_address);
			this->_socket.listen();
			sys::log_message(this->_user.name().data(), "listen");
		}

		inline sys::fd_type
		fd() const noexcept {
			return this->_socket.fd();
		}

		inline sys::port_type
		port() const noexcept {
			return sys::socket_address_cast<sys::ipv4_socket_address>(this->_address).port();
		}

		inline sys::port_type
		vnc_port() const noexcept {
			return this->_vnc_port;
		}

		void
		process(const sys::epoll_event& event) override {
			Connection::process(event);
			if (started() && event.in()) {
				if (this->_session && !this->_session->has_been_terminated()) {
					this->_session->log("refusing multiple connections");
					sys::socket tmp;
					sys::socket_address tmp2;
                    while (this->_socket.accept(tmp, tmp2)) { tmp.close(); }
				} else {
					this->_session = std::make_shared<Session>(this->_user);
					this->_session->set_port(port());
					this->_session->set_vnc_port(vnc_port());
					this->_session->verbose(this->_verbose);
                    sys::socket socket;
                    sys::socket_address address;
                    while (this->_socket.accept(socket, address)) {
                        this->parent().add(
                            new Remote_client(this->_socket, this->_session, std::move(socket), address),
                            sys::event::inout);
                        this->parent().submit(new Local_client_task(this->_session));
                    }
				}
			}
		}

	};

}

#endif // vim:filetype=cpp
