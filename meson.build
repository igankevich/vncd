project(
	'vncd',
	'cpp',
	version: '0.1.14',
	meson_version: '>=0.47',
	default_options: ['cpp_std=c++11'],
	license: 'gpl3+'
)

unistdx = dependency('unistdx', version: '>=0.44.4')
with_rpm = get_option('with_rpm')
prefix = get_option('prefix')
bindir = get_option('bindir')
sysconfdir = get_option('sysconfdir')
with_debug = get_option('with_debug')

cpp = meson.get_compiler('cpp')
cpp_args = [
    '-Werror=return-type',
    '-Werror=return-local-addr',
    '-Werror=cast-align',
    '-Werror=float-conversion',
    '-Wfloat-conversion',
    '-Wcast-align=strict',
    '-Wno-terminate',
]
if with_debug
    cpp_args += '-DVNCD_DEBUG'
else
    cpp_args += '-fvisibility-inlines-hidden'
endif

foreach arg : cpp_args
    if cpp.has_argument(arg)
        add_global_arguments(arg, language: 'cpp')
    endif
endforeach

subdir('src')

if with_rpm
	subdir('rpm')
endif
