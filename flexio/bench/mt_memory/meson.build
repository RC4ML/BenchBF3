SAMPLE_NAME_MEMORY='mt_memory'

device_source_file_memory = meson.current_source_dir() + '/device/mt_memory_device.c'
device_source_file_memory += ' '+dev_wrapper_srcs

# Build device code ./build_flexio_device.sh <app name> <source file> <output dir>
dpa_code_memory = custom_target('MT_MEMORY',
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

