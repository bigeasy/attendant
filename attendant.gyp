{
  'conditions': [
    [
      'OS == "win"', {
        'sources': [
          'attendant_win.c'
        ]
      }, {
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
            'sources': [
              'errors.c',
              'src/t/ok.c',
              'src/t/attendant/retry.t.c',
              'attendant_posix.c'
            ],
          }
        ],
      }
    ]
  ]
}
