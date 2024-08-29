{
	'variables' : {
		'node_shared': 'true',
	},
    'target_defaults': {
        'default_configuration': 'Release',
        'configurations': {
            'Debug': {
                'msvs_settings': {
                    'VCCLCompilerTool': {
                        'ExceptionHandling': 1, # /EHsc,
                        'RuntimeLibrary': '3', # /MDd
                        "AdditionalOptions": ["/std:c++17" ],
                    }
                }
		    },
            'Release': {
                'msvs_settings': {
                    'VCCLCompilerTool': {
                        'ExceptionHandling': 1, # /EHsc,
                        'RuntimeLibrary': '2', # /MD
                        "AdditionalOptions": ["/std:c++17" ],
                    }
                }
            }
        }
    }
}