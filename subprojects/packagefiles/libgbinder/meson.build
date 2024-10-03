project('libgbinder', 'c', meson_version : '>=0.56.0')

glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')
libglibutil_dep = dependency('libglibutil')
#thread_dep = dependency('threads')
#pthread_dep = dependency('pthreads')

libgbinder_deps = [glib_dep, gobject_dep, libglibutil_dep]#, thread_dep, pthread_dep]

libgbinder_sources = []
libgbinder_sources += files(
  'src/gbinder_bridge.c',
  'src/gbinder_buffer.c',
  'src/gbinder_cleanup.c',
  'src/gbinder_client.c',
  'src/gbinder_config.c',
  'src/gbinder_driver.c',
  'src/gbinder_eventloop.c',
  'src/gbinder_fmq.c',
  'src/gbinder_io_32.c',
  'src/gbinder_io_64.c',
  'src/gbinder_ipc.c',
  'src/gbinder_local_object.c',
  'src/gbinder_local_reply.c',
  'src/gbinder_local_request.c',
  'src/gbinder_log.c',
  'src/gbinder_proxy_object.c',
  'src/gbinder_reader.c',
  'src/gbinder_remote_object.c',
  'src/gbinder_remote_reply.c',
  'src/gbinder_remote_request.c',
  'src/gbinder_rpc_protocol.c',
  'src/gbinder_servicename.c',
  'src/gbinder_servicepoll.c',
  'src/gbinder_writer.c',
)

libgbinder_sources += files(
  'src/gbinder_servicemanager.c',
  'src/gbinder_servicemanager_aidl.c',
  'src/gbinder_servicemanager_aidl2.c',
  'src/gbinder_servicemanager_aidl3.c',
  'src/gbinder_servicemanager_aidl4.c',
  'src/gbinder_servicemanager_hidl.c',
)

libgbinder_sources += files(
  'src/gbinder_system.c',
)

libgbinder = library(
    'gbinder',
    sources: libgbinder_sources,
    dependencies: libgbinder_deps,
    include_directories: include_directories('include'),
)

libgbinder_dep = declare_dependency(
    dependencies: libgbinder_deps,
    include_directories: include_directories('include'),
    link_with: libgbinder,
)