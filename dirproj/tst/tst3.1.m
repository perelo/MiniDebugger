PROGRAM
NEW     @ n : 0
NEW     @ p : 1
NEW     @ s : 0
PRINT   @ "Je calcule la somme de ce que vous rentrez, jusqu'a un zero...\n"
WHILE   @ 1 (p) REPEAT
   PRINT   @ "Somme partielle : ",s,", entrez un nombre (ou zero pour terminer)... "
   READ    @ p
   COMPUTE @ s : s + p
   COMPUTE @ n : n + 1
ENDWHILE @ 1
PRINT @ "La somme des ",n," nombres rentres vaut ",s,"\n"
ENDPROGRAM
