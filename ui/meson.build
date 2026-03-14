project('planar-agent-ui', 'c',
    meson_version: '>=0.56',
    default_options: ['warning_level=1', 'c_std=c11']
)

gtk4_dep = dependency('gtk4')
webkit_dep = dependency('webkitgtk-6.0')
layer_shell_dep = dependency('gtk4-layer-shell-0')
glib_dep = dependency('glib-2.0')
json_glib_dep = dependency('json-glib-1.0')
soup_dep = dependency('libsoup-3.0')

sources = files(
    'src/main.c',
    'src/app.c',
    'ipc/compositor-ipc.c',
    'ipc/agent-ipc.c',
    'panels/panel.c',
    'panels/panel-windows.c',
    'panels/panel-chat.c',
    'panels/panel-input.c',
    'panels/panel-log.c',
    'panels/panel-notification.c',
    'panels/panel-status.c',
    'panels/panel-topbar.c',
)

inc = include_directories('src', '.')

executable('planar-agent-ui', sources,
    include_directories: inc,
    dependencies: [gtk4_dep, webkit_dep, layer_shell_dep, glib_dep, json_glib_dep, soup_dep]
)
