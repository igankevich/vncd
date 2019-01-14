#ifndef VNCD_SERVER_HH
#define VNCD_SERVER_HH

#include <iostream>
#include <memory>
#include <unordered_map>

#include <unistdx/io/poller>
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

	public:

		inline explicit
		Server(sys::port_type port):
		_port(port) {}

		inline sys::port_type
		port() const noexcept {
			return this->_port;
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

	private:
		sys::socket_address _address;
		sys::uid_type _uid;
		sys::gid_type _gid;

	public:

		inline explicit
		Remote_client(sys::socket& server_socket, sys::uid_type uid):
		_uid(uid), _gid(uid) {
			server_socket.accept(this->_socket, this->_address);
		}

		void
		process(const sys::epoll_event& event) override {
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
