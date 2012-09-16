PROGRAM
NEW @ pid : 5
NEW @ qFils : 1
NEW  @ boucle : 4
PRINT @ "Combien de boucles ? "
READ  @ boucle
FORK @ pid
NEW @ qPere : pid
NEW  @ buff : 0
NEW  @ waitFils : 1
STORE @ _$1 : waitFils
WHILE @ 1 (qPere) REPEAT
  COPY @ qFils : 0
  PRINT @ "Bonjour, je suis le pere, et mon fils a le pid ",pid,"\n"
  COPY @ qPere : 0
  WHILE @ 11 (boucle) REPEAT
    COMPUTE @ boucle : boucle - 1
    LOAD    @ buff : _$0 
    COMPUTE @ buff : buff + 1
    STORE   @ _$0 : buff
  ENDWHILE @ 11 
  WHILE @ 12 (waitFils) REPEAT
    LOAD    @ waitFils : _$1
  ENDWHILE @ 12
  LOAD    @ buff : _$0 
  PRINT @ "######  Le pere et le fils ont fini, avec ",buff," #########\n"
ENDWHILE @ 1
COMPUTE  @ pid : pid == 0
COMPUTE  @ qFils : qFils * pid
WHILE @ 2 (qFils)  REPEAT
  PRINT @ "Bonjour, je suis le fils\n"
  WHILE @ 22 (boucle) REPEAT
    COMPUTE @ boucle : boucle - 1
    LOAD    @ buff : _$0 
    COMPUTE @ buff : buff - 1
    STORE   @ _$0 : buff
  ENDWHILE @ 22 
  PRINT @ "Le fils a fini.\n"   
  COPY  @  qFils : 0
  STORE @ _$1 : qFils
ENDWHILE @ 2
ENDPROGRAM
