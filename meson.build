project('fbm-demo', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3',
    'cpp_std=c++23',
    'buildtype=release'])

src = files(
  './src/defi/uint64/orderbook.cpp',
  './src/defi/uint64/matcher.cpp',
  'src/general/funds.cpp',
)

json = subproject('json')
json_dep = json.get_variable('nlohmann_json_multiple_headers')

if get_option('emscripten')
  # Map the absolute emsdk include path to a canonical /emsdk prefix so
  # that __FILE__ strings emitted into the wasm data section don't leak
  # the user's home directory.
  em_root = run_command('em-config', 'EMSCRIPTEN_ROOT', check: false).stdout().strip()
  macro_prefix_map = []
  if em_root != ''
    macro_prefix_map = ['-fmacro-prefix-map=' + em_root + '=/emsdk']
  endif

  exe = executable('fbm', ['src/wasm_callbacks.cpp',src],
    dependencies: json_dep,
    cpp_args:['-fexceptions', '-g0',
              '-msse', '-msimd128', '-pthread',
              '-Wno-limited-postlink-optimizations', '-fPIC', '-pthread']
            + macro_prefix_map,
    name_suffix: 'html',
    include_directories: ['src'],
    link_args: ['-sMODULARIZE', '-sEXPORT_ES6',
      '-sNO_DISABLE_EXCEPTION_CATCHING',
      '-sINITIAL_MEMORY=67108864', '-mwasm64',
      '-lembind'
      ],
    install: false
  )

  # Stage the runnable bundle. `DESTDIR=$(pwd)/out meson install` lays
  # these flat in out/, mirroring demo/ exactly (the symlinks in
  # demo/ are replaced by real files here).
  install_data(meson.current_build_dir() / 'fbm.js',
               install_dir: '.', rename: 'fbm.js')
  install_data(meson.current_build_dir() / 'fbm.wasm',
               install_dir: '.', rename: 'fbm.wasm')
  install_data('demo/demo.html',
               install_dir: '.', rename: 'demo.html')
  install_data('demo/server.py',
               install_dir: '.', rename: 'server.py')
  install_data('demo/README.md',
               install_dir: '.', rename: 'README.md')
endif
