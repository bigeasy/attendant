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
      'target_name': 'retry.t',
      'type': 'executable',
      'dependencies': [
      ],
      'defines': [
      ],
      'include_dirs': [
      ],
      'sources': [
        'errors.c',
        'src/t/ok.c',
        'src/t/attendant/retry.t.c'
      ],
      'conditions': [
      [
        'OS == "win"', {
          'sources': [
            'attendant_win.c'
          ]
        }, {
          'sources': [
            'attendant_posix.c'
          ]
        }
      ]
      ]
    }
  ]
}
