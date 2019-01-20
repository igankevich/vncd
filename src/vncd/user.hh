#ifndef VNCD_USER_HH
#define VNCD_USER_HH

#include <string>
#include <utility>

#include <unistdx/util/user>

namespace vncd {

	class User {

	private:
		sys::uid_type _uid;
		sys::gid_type _gid;
		std::string _name;

	public:

		User() = default;
		User(const User&) = default;
		User(User&&) = default;
		User& operator=(const User&) = default;
		User& operator=(User&&) = default;
		~User() = default;

		inline explicit
		User(const sys::user& user):
		_uid(user.id()),
		_gid(user.group_id()),
		_name(user.name()) {}

		inline sys::uid_type
		id() const {
			return this->_uid;
		}

		inline sys::uid_type
		group_id() const {
			return this->_gid;
		}

		inline const std::string&
		name() const {
			return this->_name;
		}

		inline bool
		operator==(const User& rhs) const noexcept {
			return this->_uid == rhs._uid;
		}

		inline bool
		operator!=(const User& rhs) const noexcept {
			return !this->operator==(rhs);
		}

	};

}

namespace std {

	template <>
	struct hash<vncd::User> {

		typedef vncd::User argument_type;
		typedef sys::uid_type result_type;

		inline result_type
		operator()(const argument_type& user) const {
			return user.id();
		}

	};

}

#endif // vim:filetype=cpp
