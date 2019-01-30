#include <unistd.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <unistdx/ipc/signal>
#include <unistdx/net/socket_address>
#include <unistdx/util/group>
#include <unistdx/util/user>

#include <vncd/port.hh>
#include <vncd/server.hh>
#include <vncd/user.hh>

namespace vncd {

	template <class T>
	std::unordered_set<T>
	set_difference(
		const std::unordered_set<T>& a,
		const std::unordered_set<T>& b
	) {
		std::unordered_set<T> result(a);
		for (const auto& x : b) {
			result.erase(x);
		}
		return result;
	}

	inline void
	operator>>(const char* arg, std::chrono::seconds& t) {
		long tmp;
		if (!(std::stringstream(arg) >> tmp) || tmp <= 0) {
			throw std::invalid_argument("bad duration");
		}
		t = std::chrono::seconds(tmp);
	}

	class Update_users: public Task {

	public:
		typedef std::unordered_set<User> set_type;

	private:
		Server& _server;
		std::string _group;
		Port _port = 50000;
		Port _vnc_base_port = 40000;
		sys::socket_address _address;
		set_type _old_users;
		std::chrono::seconds _tcp_user_timeout{60};
		std::chrono::seconds _update_period{30};

	public:

		inline explicit
		Update_users(Server& server): _server(server) {
			this->period(this->_update_period);
			this->repeat_forever();
		}

		void
		parse_arguments(int argc, char* argv[]) {
			for (int opt; (opt = ::getopt(argc, argv, "hg:p:P:t:T:")) != -1;) {
				switch (opt) {
				case 'h':
					usage();
					std::exit(EXIT_SUCCESS);
				case 'g':
					this->_group = ::optarg;
					break;
				case 'p':
					::optarg >> this->_port;
					break;
				case 'P':
					::optarg >> this->_vnc_base_port;
					break;
				case 't':
					::optarg >> this->_tcp_user_timeout;
					break;
				case 'T':
					::optarg >> this->_update_period;
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
			this->_server.set_user_timeout(this->_tcp_user_timeout);
		}

		void
		usage() {
			std::cout <<
				"usage: vncd [-h] [-p PORT] [-P PORT] [-t TIMEOUT] [-T PERIOD]"
				" -g GROUP [ADDRESS]\n"
				"    -p  input port\n"
				"    -P  output port\n"
				"    -t  TCP user timeout\n"
				"    -T  update period\n";
		}

		void
		run() override {
			auto new_users = find_new_users();
			auto users_to_add = set_difference(new_users, this->_old_users);
			auto users_to_remove = set_difference(this->_old_users, new_users);
			this->_old_users = std::move(new_users);
			for (const auto& user : users_to_remove) {
				Port port = this->_port + user.id();
				this->_server.remove(port);
			}
			for (const auto& user : users_to_add) {
				Port port = this->_port + user.id();
				Port vnc_port = this->_vnc_base_port + user.id();
				sys::socket_address address{this->_address, port};
				this->_server.add(new Local_server(address, vnc_port, user));
			}
		}

	private:

		set_type
		find_new_users() {
			sys::group group;
			if (!sys::find_group(_group.data(), group)) {
				throw std::invalid_argument("unknown group");
			}
			sys::uid_type overflow_uid = 65534;
			if (!(std::stringstream("/proc/sys/fs/overflowuid") >> overflow_uid)) {
				overflow_uid = 65534;
			}
			sys::gid_type overflow_gid = 65534;
			if (!(std::stringstream("/proc/sys/fs/overflowgid") >> overflow_gid)) {
				overflow_gid = 65534;
			}
			set_type result;
			for (const auto& member : group) {
				try {
					sys::user user;
					if (!sys::find_user(member, user)) {
						throw std::invalid_argument("unknown user in group");
					}
					if (user.id() < 1000 || user.group_id() < 1000) {
						throw std::invalid_argument(
							"will not work for unpriviledged user"
						);
					}
					if (user.id() == overflow_uid || user.group_id() == overflow_gid) {
						throw std::invalid_argument(
							"will not work for overflow user/group"
						);
					}
					result.emplace(user);
				} catch (const std::exception& err) {
					sys::log_message(
						"server",
						"skipping user _: _",
						member,
						err.what()
					);
				}
			}
			return result;
		}

	};

}

int
main(int argc, char* argv[]) {
	using namespace vncd;
	int ret = EXIT_SUCCESS;
	try {
		sys::this_process::ignore_signal(sys::signal::child);
		sys::this_process::ignore_signal(sys::signal::broken_pipe);
		Server server;
		std::unique_ptr<Update_users> update_users(new Update_users(server));
		update_users->parse_arguments(argc, argv);
		server.submit(std::move(update_users));
		server.run();
	} catch (const std::exception& err) {
		std::cerr << err.what() << std::endl;
		ret = EXIT_FAILURE;
	}
	return ret;
}
