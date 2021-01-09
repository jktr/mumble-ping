project(
  'mumble-ping',
  'c',
  default_options: [
    'c_std=c17',
    'warning_level=2',
    'werror=true',
  ],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')

executable ('mumble-ping', 'main.c')
