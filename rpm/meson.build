cbd = meson.current_build_dir()
# RPM archive {{{
archive_name = meson.project_name() + '-' + meson.project_version()
full_archive_name = archive_name + '.tar.xz'
run_target(
	'rpm-archive',
	command: [
		'git',
		'-C',
		meson.source_root(),
		'archive',
		'-o',
		join_paths(cbd, full_archive_name),
		'--prefix',
		archive_name + '/',
		'HEAD'
	]
)
# }}}
# RPM spec {{{
rpm_spec = configuration_data()
rpm_spec.set('name', meson.project_name())
rpm_spec.set('version', meson.project_version())
rpm_spec.set('release', '1')
rpm_spec.set('archive_name', full_archive_name)
rpm_spec.set('arch', build_machine.cpu_family())
rpm_spec.set('license', ' '.join(meson.project_license()))
configure_file(
	input: meson.project_name() + '.spec.in',
	output: meson.project_name() + '.spec',
	configuration: rpm_spec
)
# }}}
# RPM build {{{
rpm_setup_tree = find_program('rpmdev-setuptree', required: false)
if not rpm_setup_tree.found()
	rpm_setup_tree = find_program('rpmsetuptree', required: false)
endif
if rpm_setup_tree.found()
	run_target('rpm-setup-tree', command: rpm_setup_tree)
endif
rpm_build = find_program('rpmbuild', required: false)
if rpm_build.found()
	run_target(
		'rpm-build',
		command: [
			rpm_build,
			'-ba',
			join_paths(cbd, meson.project_name() + '.spec'),
			'--define',
			'_sourcedir ' + cbd,
			'--define',
			'_rpmdir ' + join_paths(cbd, 'RPMS')
		]
	)
endif
# }}}
# RPM target {{{
ninja = find_program('ninja')
if ninja.found()
	run_target(
		'rpm',
		command: [
			ninja,
			'-C',
			meson.build_root(),
			'rpm-setup-tree',
			'rpm-archive',
			'rpm-build'
		]
	)
endif
# }}}

# sysconfig {{{
sysconfig = configuration_data()
configure_file(
	input: 'vncd.sysconfig.in',
	output: 'vncd.sysconfig',
	copy: true
)
# }}}

# service {{{
service = configuration_data()
service.set('prefix', prefix)
service.set('bindir', bindir)
service.set('sysconfdir', sysconfdir)
configure_file(
	input: 'vncd.service.in',
	output: 'vncd.service',
	configuration: service
)
# }}}

# rsync {{{
rsync = find_program('rsync')
if rsync.found()
	run_target(
		'deploy',
		command: [
			rsync,
			'-acvP',
			join_paths(cbd, 'RPMS', build_machine.cpu_family()) + '/',
			url
		]
	)
endif
# }}}
