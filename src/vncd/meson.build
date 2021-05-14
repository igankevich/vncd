vncd_src = [
	'main.cc'
]

vncd_deps = [
	unistdx
]

executable(
	'vncd',
	sources: vncd_src,
	dependencies: vncd_deps,
	include_directories: src,
    implicit_include_directories: false,
	install: true
)

subdir('test')
