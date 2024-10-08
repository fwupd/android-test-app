project('fwupd-poc-binder', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

sdk_version_command = run_command('adb', '-d', 'shell', 'getprop', 'ro.build.version.sdk')

sdk_version = sdk_version_command.returncode() == 0 ? sdk_version_command.stdout().strip() : '34' # Default to Android 14

conf_data = configuration_data()
conf_data.set('sdk_version', sdk_version)
configure_file(input: 'gbinder.conf.in',
  output: 'gbinder.conf',
  configuration: conf_data,
)

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(['-Wno-unused-parameter']), language: 'c')

libgbinder_dep = dependency('libgbinder')
libglibutil = dependency('libglibutil')
glib_dep = dependency('glib-2.0')

poc_deps = [glib_dep, libglibutil, libgbinder_dep]


exe = executable('fwupd-poc-binder', 'fwupd_poc_binder.c',
  dependencies: poc_deps,
  install : true,
)

service_list_exe = executable('poc-service-list', 'poc_service_list.c',
  dependencies: poc_deps,
  install : true,
)

test('basic', exe)
