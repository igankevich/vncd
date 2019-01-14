#ifndef VNCD_SERVER_HH
#define VNCD_SERVER_HH

#include <iostream>
#include <memory>
#include <unordered_map>

#include <unistdx/base/log_message>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/identity>
#include <unistdx/ipc/process_group>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

namespace vncd {

	class Connection;
	class Local_server;
	class Remote_client;
	class Server;

	struct No_lock {
		inline void
		lock() {}
		inline void
		unlock() {}
	};

	class Connection {

	protected:
		Server* _parent = nullptr;
		sys::socket _socket;

	public:

		Connection() = default;

		inline explicit
		Connection(sys::family_type family):
		_socket(family) {}

		virtual ~Connection() {}

		virtual void
		process(const sys::epoll_event& event) = 0;

		inline void
		parent(Server* rhs) {
			this->_parent = rhs;
		}

		inline sys::fd_type
		fd() const noexcept {
			return this->_socket.fd();
		}

	};


	class Server {

	private:
		friend class Local_server;

	private:
		typedef std::unique_ptr<Connection> connection_pointer;

	private:
		sys::event_poller _poller;
		std::unordered_map<sys::fd_type,connection_pointer> _connections;
		sys::port_type _port;
		sys::port_type _vnc_base_port;

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
			auto fd = connection->fd();
			this->_connections.emplace(fd, connection);
			this->_poller.emplace(fd, sys::event::in);
		}

		void
		run() {
			No_lock lock;
			this->_poller.wait(
				lock,
				[this] () {
				    for (const auto& event : this->_poller) {
				        auto result = this->_connections.find(event.fd());
				        if (result == this->_connections.end()) {
				            std::clog << "bad fd: " << event.fd() << std::endl;
				            continue;
						}
				        result->second->process(event);
					}
				    return false;
				}
			);
		}

	};

	/// VNC remote client that connects to one of the local servers.
	class Remote_client: public Connection {

	public:
		typedef sys::basic_fildesbuf<char,std::char_traits<char>,sys::fd_type>
		    streambuf_type;

		enum class State {
			Initial,
			Starting_VNC_server
		};

	private:
		sys::socket_address _address;
		sys::uid_type _uid;
		sys::gid_type _gid;
		streambuf_type _buffer;
		State _state = State::Initial;
		sys::process_group _vnc;

	public:

		inline explicit
		Remote_client(sys::socket& server_socket, sys::uid_type uid):
		_uid(uid), _gid(uid) {
			server_socket.accept(this->_socket, this->_address);
			this->_buffer.setfd(this->fd());
		}

		void
		process(const sys::epoll_event& event) override {
			if (event.in()) {
				this->_buffer.pubfill();
			}
			switch (this->_state) {
			case State::Initial:
				this->_state = State::Starting_VNC_server;
				this->vnc_start();
				break;
			case State::Starting_VNC_server:
				// TODO add local vnc client to server
				break;
			}
		}

	private:

		void
		vnc_start() {
			try {
				this->_vnc.emplace([this] () {this->vnc_main();});
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
			::setenv("VNCD_PORT", std::to_string(this->vnc_port()).data(), 1);
			sys::this_process::execute(args);
		}

		inline sys::port_type
		vnc_port() const {
			return this->_parent->vnc_base_port() + this->_uid;
		}

	};

	/// Local server that accepts connections on a particular port.
	class Local_server: public Connection {

	private:
		sys::socket_address _address;

	public:

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
			if (event.in()) {
				sys::uid_type uid = this->_parent->port() + this->port();
				std::clog << "user " << uid << " connected" << std::endl;
				this->_parent->add(new Remote_client(this->_socket, uid));
			}
		}

	};

}

#endif // vim:filetype=cpp
