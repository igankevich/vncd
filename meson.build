project(
	'vncd',
	'cpp',
	version: '0.1.3',
	meson_version: '>=0.47',
	default_options: ['cpp_std=c++11'],
	license: 'GPLv2'
)

unistdx = dependency('unistdx', version: '>=0.4.15')
with_rpm = get_option('with_rpm')
url = get_option('url')
prefix = get_option('prefix')
bindir = get_option('bindir')
sysconfdir = get_option('sysconfdir')

if get_option('buildtype').contains('debug')
	add_global_arguments('-DVNCD_DEBUG', language: 'cpp')
else
	add_global_arguments('-DNDEBUG', language: 'cpp')
endif

subdir('src')

if with_rpm
	subdir('rpm')
endif
