#ifndef VNCD_PORT_HH
#define VNCD_PORT_HH

#include <sstream>
#include <stdexcept>

#include <unistdx/net/socket_address>

namespace vncd {

	inline bool
	is_valid_port(int port) {
		return 0L < port && port < 65536L;
	}

	inline void
	check_port(int port) {
		if (!is_valid_port(port)) {
			throw std::invalid_argument("bad port");
		}
	}

	class Port {

	private:
		sys::port_type _port = 0;

	public:

		Port() = default;

		inline
		Port(sys::port_type port):
		_port(port) {
			check_port(static_cast<int>(port));
		}

		inline
		operator sys::port_type() const {
			return this->_port;
		}

		inline Port&
		operator=(sys::port_type port) {
			this->_port = port;
			return *this;
		}

	};

	inline void
	operator>>(const char* arg, Port& port) {
		long tmp;
		if (!(std::stringstream(arg) >> tmp) || !is_valid_port(tmp)) {
			throw std::invalid_argument("bad port");
		}
		port = static_cast<sys::port_type>(tmp);
	}

}


#endif // vim:filetype=cpp
