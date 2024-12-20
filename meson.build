project('fwupd-poc-binder', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23'])

conf_data = configuration_data()

sdk_version_command = run_command('adb', '-d', 'shell', 'getprop', 'ro.build.version.sdk')

sdk_version = sdk_version_command.returncode() == 0 ? sdk_version_command.stdout().strip() : '34' # Default to Android 14

conf_data = configuration_data()
conf_data.set('sdk_version', sdk_version)


cc = meson.get_compiler('c')
#target_triple = run_command(cc.cmd_array(), '-dumpmachine', check: true).stdout()
#sys_root = meson.get_external_property('sys_root', '/opt/android/android-ndk-r27/toolchains/llvm/prebuilt/linux-x86_64/sysroot/')
add_project_arguments(cc.get_supported_arguments(['-Wno-unused-parameter']), language: 'c')

libgbinder_dep = dependency('libgbinder')
libglibutil = dependency('libglibutil')
glib_dep = dependency('glib-2.0')

binder_ndk = cc.find_library('binder_ndk',
#  dirs: [sys_root / 'usr/lib' / target_triple / sdk_version],
#  header_include_directories: include_directories(sys_root / 'usr/include'),
  dirs: [ meson.current_source_dir() / 'lib_ndk' ],
  header_include_directories: include_directories( 'include_platform' ),
  required: false,
  )
# binder_ndk = define_dependency('binder_ndk')
conf_data.set10('HAS_BINDER_NDK', binder_ndk.found() and
  cc.has_header('android/binder_status.h') and
  cc.has_header('android/binder_parcel.h') and
  cc.has_header('android/persistable_bundle.h') and
  cc.has_header_symbol('android/binder_parcel.h', 'AParcel_create', dependencies: binder_ndk) and
  cc.has_header_symbol('android/binder_parcel.h', 'AParcel_marshal', dependencies: binder_ndk) and
  cc.has_header_symbol('android/persistable_bundle.h', 'APersistableBundle_new', dependencies: binder_ndk)
  )

poc_deps = [glib_dep, libglibutil, libgbinder_dep, binder_ndk]

configure_file(input: 'gbinder.conf.in',
  output: 'gbinder.conf',
  configuration: conf_data,
)
configure_file(output: 'config.h',
  configuration: conf_data)

exe = executable('fwupd-poc-binder', ['fwupd_poc_binder.c', get_option('fwupd-path') / 'src/gparcelable.c'],
  dependencies: poc_deps,
  include_directories: include_directories('../fwupd/src/'),
  install : true,
)

exe_ndk = executable('simple-fd-svc', ['simple_fd_svc.c', get_option('fwupd-path') / 'src/gparcelable.c'],
  dependencies: [glib_dep, binder_ndk],
  include_directories: include_directories('../fwupd/src/', 'include_platform'),
  install : true,
)

service_list_exe = executable('poc-service-list', 'poc_service_list.c',
  dependencies: poc_deps,
  install : true,
)

test('basic', exe)
