project('fbm-demo', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3',
    'cpp_std=c++23'])

src = files(
  './src/defi/uint64/orderbook.cpp',
  './src/defi/uint64/matcher.cpp',
  'src/general/funds.cpp',
)

json = subproject('json')
json_dep = json.get_variable('nlohmann_json_multiple_headers')

if get_option('emscripten')
  exe = executable('demo', ['src/wasm_callbacks.cpp',src],
    dependencies: json_dep,
    cpp_args:['-fexceptions','-msse', '-msimd128', '-pthread',  '-Wno-limited-postlink-optimizations',  '-fPIC', '-pthread'],
    name_suffix: 'html',
    include_directories: ['src'],
    link_args: ['-sMODULARIZE', '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToNewUTF8', '-sEXPORT_ES6',
      '-sNO_DISABLE_EXCEPTION_CATCHING', 
      '-sINITIAL_MEMORY=67108864', '-sEXPORT_ALL=1', '-mwasm64'
      ])
endif
