// SPDX-License-Identifier: gpl3+

#include <cstdlib>

#include <unistdx/base/byte_buffer>
#include <unistdx/base/log_message>
#include <unistdx/io/poller>
#include <unistdx/net/socket>
#include <unistdx/net/socket_address>

#include <vncd/port.hh>

#include <thread>

namespace vncd {

	struct No_lock {
		void lock() {}
		void unlock() {}
	};

	class Server {

	private:
		sys::socket _server{sys::family_type::inet};
		sys::event_poller _poller;
		sys::socket _client;
		sys::byte_buffer _buffer{4096*10};

	public:

		Server() {
			const char* str = std::getenv("VNCD_PORT");
			if (!str) {
				throw std::invalid_argument("bad vnc port");
			}
			Port port;
			str >> port;
			sys::socket_address address{{127,0,0,1},port};
			sys::log_message("stub", "listen _", address);
			this->_server.bind(address);
			this->_server.listen();
			this->_poller.emplace(this->_server.fd(), sys::event::in);
		}

		void
		run() {
			using namespace std::chrono;
			using namespace std::this_thread;
			sleep_for(seconds(10));
			No_lock lock;
			sys::log_message("stub", "wait");
			this->_poller.wait(lock, [this] () { return process_events(); });
			sys::log_message("stub", "end");
		}

		bool
		process_events() {
			for (const auto& event : this->_poller) {
				sys::log_message("stub", "event _", event);
				if (event.fd() == this->_server.fd()) {
					sys::socket_address address;
					this->_server.accept(this->_client, address);
					sys::log_message("stub", "accepted connection from _", address);
					this->_poller.emplace(this->_client.fd(), sys::event::in);
				} else {
					if (event.bad()) {
						sys::log_message("stub", "connection closed");
						break;
					}
					if (event.in()) {
						ssize_t nread = 0;
						nread += this->_client.read(
							this->_buffer.data(),
							this->_buffer.size()
						);
						ssize_t nwritten = 0;
						while (nread != nwritten) {
							nwritten += this->_client.write(
								this->_buffer.data() + nwritten,
								nread
							);
						}
					}
				}
			}
			return false;
		}

	};

}

int main() {
	using namespace vncd;
	Server server;
	server.run();
	return 0;
}
