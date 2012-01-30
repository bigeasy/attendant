{
  'targets': [
    {
      'target_name': 'scripts',
      'type': 'none',
      'copies': [
        {
          'destination': 't/bin',
          'files': [ 'src/t/bin/when' ],
        },
        {
          'destination': 't/server',
          'files': [ 'src/t/server/default.t' ],
        }
      ]
    },
    {
      'target_name': 'attendant',
      'type': 'executable',
      'dependencies': [
      ],
      'defines': [
      ],
      'include_dirs': [
      ],
      'sources': [
        'attendant.c',
        'errors.c',
        'src/t/attendant/retry.t.c',
      ],
      'conditions': [
      ]
    }
  ]
}
