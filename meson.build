project(
	'vncd',
	'cpp',
	version: '0.1.0',
	meson_version: '>=0.47',
	default_options: ['cpp_std=c++11'],
	license: 'GPLv2'
)

unistdx = dependency('unistdx', version: '>=0.4.15')
with_rpm = get_option('with_rpm')

prefix = get_option('prefix')
bindir = get_option('bindir')
sysconfdir = get_option('sysconfdir')

subdir('src')

if with_rpm
	subdir('rpm')
endif
