project(
	'vncd',
	'cpp',
	version: '0.1.0',
	meson_version: '>=0.46',
	default_options: ['cpp_std=c++11']
)

unistdx = dependency('unistdx', version: '>=0.4.15')
with_rpm = get_option('with_rpm')

subdir('src')

if with_rpm
	subdir('rpm')
endif
