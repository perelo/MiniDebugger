PROGRAM
NEW @ pid : 5
NEW @ qFils : 1
FORK @ pid
PRINT @ "apres FORK\n"
NEW @ qPere : pid
NEW  @ waitFils : 1
STORE @ _$1 : waitFils
WHILE @ 1 (qPere) REPEAT
  COPY @ qFils : 0
  PRINT @ "Bonjour, je suis le pere, et mon fils a le pid ",pid,"\n"
  COPY @ qPere : 0
  WHILE @ 12 (waitFils) REPEAT
    LOAD    @ waitFils : _$1
  ENDWHILE @ 12
  PRINT @ "######  Le pere et le fils ont fini  #########\n"
ENDWHILE @ 1
COMPUTE  @ pid : pid == 0
COMPUTE  @ qFils : qFils * pid
WHILE @ 2 (qFils)  REPEAT
  PRINT @ "Bonjour, je suis le fils\n"
  PRINT @ "Le fils a fini.\n"   
  COPY  @  qFils : 0
  STORE @ _$1 : qFils
ENDWHILE @ 2
ENDPROGRAM
