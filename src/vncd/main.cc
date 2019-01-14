#include <unistd.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistdx/net/socket_address>
#include <unistdx/util/group>
#include <unistdx/util/user>

#include <vncd/server.hh>

class VNCd {

private:
	std::string _group;
	sys::port_type _port = 50000;
	sys::port_type _vnc_base_port = 40000;
	sys::socket_address _address;

public:

	void
	parse_arguments(int argc, char* argv[]) {
		for (int opt; (opt = ::getopt(argc, argv, "hg:p:P:")) != -1;) {
			switch (opt) {
			case 'h':
				usage();
				std::exit(EXIT_SUCCESS);
			case 'g':
				this->_group = ::optarg;
				break;
			case 'p':
				read_port(::optarg, this->_port);
				break;
			case 'P':
				read_port(::optarg, this->_vnc_base_port);
				break;
			default:
				usage();
				std::exit(EXIT_FAILURE);
			}
		}
		if (this->_group.empty()) {
			throw std::invalid_argument("bad group");
		}
		if (::optind+1 < argc) {
			throw std::invalid_argument("trailing arguments");
		}
		if (::optind+1 == argc) {
			std::stringstream tmp;
			tmp << argv[::optind] << ":0";
			tmp >> this->_address;
			if (!tmp) {
				throw std::invalid_argument("bad address");
			}
		}
		if (!std::getenv("VNCD_SERVER")) {
			throw std::invalid_argument("VNCD_SERVER variable is not set");
		}
		if (!std::getenv("VNCD_SESSION")) {
			throw std::invalid_argument("VNCD_SESSION variable is not set");
		}
	}

	void
	usage() {
		std::cout << "usage: vncd [-h] [-p PORT] -g GROUP [ADDRESS]\n";
	}

	void
	run() {
		using namespace vncd;
		std::clog << "_group=" << _group << std::endl;
		std::clog << "_port=" << _port << std::endl;
		std::clog << "_address=" << _address << std::endl;
		sys::group group;
		if (!sys::find_group(_group.data(), group)) {
			throw std::invalid_argument("unknown group");
		}
		Server server(this->_port, this->_vnc_base_port);
		sys::uid_type overflow_uid = 65534;
		if (!(std::stringstream("/proc/sys/fs/overflowuid") >> overflow_uid)) {
			overflow_uid = 65534;
		}
		sys::gid_type overflow_gid = 65534;
		if (!(std::stringstream("/proc/sys/fs/overflowgid") >> overflow_gid)) {
			overflow_gid = 65534;
		}
		for (const auto& member : group) {
			sys::user user;
			if (!sys::find_user(member, user)) {
				throw std::invalid_argument("unknown user in group");
			}
			if (user.id() < 1000 || user.group_id() < 1000) {
				throw std::invalid_argument("will not work for unpriviledged user");
			}
			if (user.id() == overflow_uid || user.group_id() == overflow_gid) {
				throw std::invalid_argument("will not work for overflow user/group");
			}
			long new_port = this->_port + user.id();
			if (new_port <= 0 || new_port > 65535) {
				throw std::invalid_argument("bad port");
			}
			sys::port_type port = static_cast<sys::port_type>(new_port);
			sys::socket_address address{this->_address, port};
			std::clog << "address=" << address << std::endl;
			server.add(new Local_server(address));
		}
		server.run();
	}

	void
	read_port(const char* arg, sys::port_type& port) {
		long tmp;
		if (!(std::stringstream(arg) >> tmp) || tmp <= 0 || tmp > 65535) {
			throw std::invalid_argument("bad port");
		}
		this->_port = static_cast<sys::port_type>(tmp);
	}

};

int
main(int argc, char* argv[]) {
	VNCd vncd;
	int ret = EXIT_SUCCESS;
	try {
		vncd.parse_arguments(argc, argv);
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
