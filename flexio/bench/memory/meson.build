SAMPLE_NAME_CACHE='bench_cache'

device_source_file_cache = meson.current_source_dir() + '/device/bench_cache_device.c'
device_source_file_cache += ' '+dev_wrapper_srcs

# Build device code ./build_flexio_device.sh <app name> <source file> <output dir>
dpa_code_cache = custom_target('BENCH_CACHE',
		command : [device_builder,
			SAMPLE_NAME_CACHE + '_device',
			device_source_file_cache,
			meson.current_build_dir(),
			dev_inc_dir],
		output : SAMPLE_NAME_CACHE + '_device.a')
#The flexio_app variable in the code must be the same with the above SAMPLE_NAME_CACHE+'_device'
dpa_dependency_cache = static_library(SAMPLE_NAME_CACHE + '_device', link_with : dpa_code_cache)

#Build host code

srcs_cache = [
        SAMPLE_NAME_CACHE + '.cpp',
		wrapper_srcs,
]

executable(SAMPLE_NAME_CACHE,
	srcs_cache,
	link_with: [dpa_dependency_cache],
	dependencies: sample_dependencies,
	include_directories: [top_inc,util_dir],
	cpp_args: ['-DRUN_IN_DPA'],
)



SAMPLE_NAME_MEMORY='bench_memory'

device_source_file_memory = meson.current_source_dir() + '/device/bench_memory_device.c'
device_source_file_memory += ' '+dev_wrapper_srcs

# Build device code ./build_flexio_device.sh <app name> <source file> <output dir>
dpa_code_memory = custom_target('BENCH_MEMORY',
		command : [device_builder,
			SAMPLE_NAME_MEMORY + '_device',
			device_source_file_memory,
			meson.current_build_dir(),
			dev_inc_dir],
		output : SAMPLE_NAME_MEMORY + '_device.a')
#The flexio_app variable in the code must be the same with the above SAMPLE_NAME_MEMORY+'_device'
dpa_dependency_memory = static_library(SAMPLE_NAME_MEMORY + '_device', link_with : dpa_code_memory)

#Build host code

srcs_memory = [
        SAMPLE_NAME_MEMORY + '.cpp',
		wrapper_srcs,
]

executable(SAMPLE_NAME_MEMORY,
	srcs_memory,
	link_with: [dpa_dependency_memory],
	dependencies: sample_dependencies,
	include_directories: [top_inc,util_dir],
)



#Build X86/ARM bench

flexio_host_inc = include_directories('/opt/mellanox/flexio/include/')
doca_host_inc = include_directories('/opt/mellanox/doca/include/')
cache_srcs = [
	'bench_cache_no_dpa.cpp',
	join_paths(meson.project_source_root(), 'utils/numautil.cpp'),
]


host_dependencies = [dependency('numa')]
host_dependencies += dependency('doca')

executable('cache_host',
	cache_srcs,
	cpp_args:'-O3',
	include_directories:[top_inc,flexio_host_inc, doca_host_inc],
	dependencies: host_dependencies
)
