project('cairo-dwrite-font-view', 'cpp',
    version: '0.1')

cairo_dep = dependency('cairo-win32-dwrite-font')

windows = import('windows')
res_dep = windows.compile_resources('RenderTest.rc')

add_project_arguments('-DUNICODE', language: ['cpp'])

executable('cairo-dwrite-font-view',
    ['Common.h',
    'DWriteRenderer.h',
    'D2DRenderer.cpp',
    'IRenderer.h',
    'D2DRenderer.h',
    'RenderTest.cpp',
    'TextHelpers.cpp',
    'resource.h',
    'DWriteRenderer.cpp',
    'RenderTest.h',
    'TextHelpers.h',
    res_dep],
    dependencies: [cairo_dep],
    gui_app: true,
    install: true)
