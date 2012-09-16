PROGRAM
NEW      @ n : 0
NEW      @ p : 1
PRINT    @ "Je calcule la somme de ce que vous rentrez, jusqu'a un zero...\n"
WHILE    @ 1 (p) REPEAT
   PRINT   @ "entrez un nombre (ou zero pour terminer)... "
   READ    @ p
   STORE   @ _$n : p
   COMPUTE @ n : n + 1
ENDWHILE @ 1
NEW      @ k : 0
NEW      @ s : 0
WHILE    @ 2 (k < n) REPEAT
   PRINT   @ "Retrouvant l'element ",k,"...\n"
   LOAD    @ p : _$k
   COMPUTE @ s : s + p
   COMPUTE @ k : k + 1
ENDWHILE @ 2
PRINT    @ "La somme des ",n," nombres rentres vaut ",s,"\n"
ENDPROGRAM
