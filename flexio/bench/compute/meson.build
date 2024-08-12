SAMPLE_NAME='bench_compute'

device_source_file = meson.current_source_dir() + '/device/bench_compute_device.c'
device_source_file += ' '+meson.current_source_dir() + '/dhrystone/dhrystone_cross.c'
device_source_file += ' '+dev_wrapper_srcs

# Build device code ./build_flexio_device.sh <app name> <source file> <output dir>
dpa_code = custom_target('BENCH_COMPUTE',
		command : [device_builder,
			SAMPLE_NAME + '_device',
			device_source_file,
			meson.current_build_dir(),
			dev_inc_dir],
		output : SAMPLE_NAME + '_device.a')
#The flexio_app variable in the code must be the same with the above SAMPLE_NAME+'_device'
dpa_dependency = static_library(SAMPLE_NAME + '_device', link_with : dpa_code)


#Build host code

srcs = [
        SAMPLE_NAME + '.cpp',
		wrapper_srcs,
]

executable(SAMPLE_NAME,
	srcs,
	link_with: [dpa_dependency],
	dependencies: sample_dependencies,
	include_directories: [top_inc,util_dir],
)


#Build X86/ARM bench
dhrystone_srcs = [
	'dhrystone/dhrystone_main.c',
	'dhrystone/dhrystone_cross.c',
]
executable('dhrystone',
	dhrystone_srcs,
	c_args:'-O0'
)