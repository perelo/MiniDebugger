PROGRAM
SIGNAL
PRINT @ "dans le traitant\n"
PRINT @ "encore in handler\n"
ENDSIGNAL
PRINT @ "Hello world\n"
SIGADD @ 2
PRINT @ "Bonjour le monde\n"
PRINT @ "REBONJOUR\n"
ENDPROGRAM
