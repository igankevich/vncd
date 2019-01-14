#include <unistd.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistdx/net/socket_address>
#include <unistdx/util/group>
#include <unistdx/util/user>

class VNCd {

private:
	std::string _group;
	sys::port_type _port = 50000;
	std::vector<sys::port_type> _ports;

public:

	void
	parse_arguments(int argc, char* argv[]) {
		for (int opt; (opt = ::getopt(argc, argv, "hg:p:")) != -1;) {
			switch (opt) {
			case 'h':
				usage();
				std::exit(EXIT_SUCCESS);
			case 'g':
				this->_group = ::optarg;
				break;
			case 'p':
				long tmp;
				if (!(std::stringstream(::optarg) >> tmp) || tmp <= 0 || tmp > 65535) {
					throw std::invalid_argument("bad port");
				}
				this->_port = static_cast<sys::port_type>(tmp);
				break;
			default:
				usage();
				std::exit(EXIT_FAILURE);
			}
		}
	}

	void
	validate_arguments() {
		if (this->_group.empty()) {
			throw std::invalid_argument("bad group");
		}
	}

	void
	usage() {
		std::cout << "usage: vncd [-h] [-p PORT] -g GROUP\n";
	}

	void
	run() {
		std::clog << "_group=" << _group << std::endl;
		std::clog << "_port=" << _port << std::endl;
		sys::group group;
		if (!sys::find_group(_group.data(), group)) {
			throw std::invalid_argument("unknown group");
		}
		for (const auto& member : group) {
			sys::user user;
			if (!sys::find_user(member, user)) {
				throw std::invalid_argument("unknown user in group");
			}
			long new_port = this->_port + user.id();
			if (new_port <= 0 || new_port > 65535) {
				throw std::invalid_argument("bad port");
			}
			this->_ports.push_back(static_cast<sys::port_type>(new_port));
			std::clog << "_ports.back()=" << _ports.back() << std::endl;
		}
	}

};

int
main(int argc, char* argv[]) {
	VNCd vncd;
	int ret = EXIT_SUCCESS;
	try {
		vncd.parse_arguments(argc, argv);
		vncd.validate_arguments();
		vncd.run();
	} catch (const std::invalid_argument& err) {
		vncd.usage();
		std::cerr << err.what() << std::endl;
		ret = EXIT_FAILURE;
	} catch (const std::exception& err) {
		std::cerr << err.what() << std::endl;
		ret = EXIT_FAILURE;
	}
	return ret;
}
