// SPDX-License-Identifier: gpl3+

#ifndef VNCD_USER_HH
#define VNCD_USER_HH

#include <string>
#include <utility>

#include <unistdx/system/nss>

namespace vncd {

    class User {

    private:
        sys::uid_type _uid;
        sys::gid_type _gid;
        std::string _name;
        std::string _home;
        std::string _shell;

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
        _name(user.name()),
        _home(user.home()),
        _shell(user.shell())
        {}

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

        inline const std::string&
        home() const {
            return this->_home;
        }

        inline const std::string&
        shell() const {
            return this->_shell;
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
