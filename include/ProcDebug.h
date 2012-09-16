/**
 *
 * @File : ProcDebug.h
 *
 * @Author : A. B. Dragut
 *
 * @Date : 28/11/2011
 *
 * @Synopsis : debugger-emulateur
 *
 **/


#ifndef __PROCDEBUG_H__
#define __PROCDEBUG_H__
#include <vector> 
#include <deque> 
#include <string> 
#include <map> 

namespace ProcDebug {
  
  class Scheduler; // car ProcInfo a un pointeur dessus
  // cette classe sera definie plus bas
  // 
  // elements du minilangage
  //   nombres (uniquement entiers), 
  //   operations arithmetiques usuelles
  //   convention C pour les boolens (0 - faux, != 0 vrai) 
  //   operations de comparaison usuelles
  //   pour faire la negation : 'non a' se calcule en faisant a == 0
  //   le ET logique est l'addition de nombres positifs ou zero
  //   le OU logique est la multiplication de nombres positifs ou zero
  //   variables (noms comme en C/C++, mais sans '_'),uniquement de type entier
  //   tableaux avec la syntaxe a$2 pour l'equivalent a[2] en C/C++
  //     (a$b etant permis egalement) 
  //   constantes chaines de caracteres, pour le PRINT uniquement
  //
  //   l'identificateur '_' est reserve pour les ressources partagees
  //     comme la memoire partagee (un seul segment, disponible pour tous)
  //     ou le semaphore (un seul mutex, egalement disponible pour tous) 
  // syntaxe du minilangage (les espaces blancs ne comptent pas du tout)
  // PROGRAM 
  // ... instructions (simples ou complexes)...
  // ENDPROGRAM
  //
  // chaque programme/processus a simuler comporte des instructions comme suit
  // (une seule instruction simple par ligne, toutes en majuscules)
  //  Instructions simples 
  // 
  //    . NEW     @ <Variable>       : <nbrOuAutreVariableDejaDefinie>
  //    . READ    @ <Variable>
  //    . COMPUTE @ <Variable>       : <nbrOuVar> <oper> <nbrOuVar>
  //    . COPY    @ <Variable>       : <nbrOuVar>
  //    . STORE   @ <Var>$<nbrOuVar> : <nbrOuVar>  
  //    . LOAD    @ <nbrOuVar>       : <Var>$<nbrOuVar> 
  //    . PRINT   @ <nbrOuVarOuString>,<nbrOuVarOuString>,...
  //    . FORK    @ <Var>            // 0 si fils, pid du fils (int) si pere
  //    . MUTEX   @ _ : _P            
  //    . MUTEX   @ _ : _V
  //
  //   Pour le STORE ou le LOAD on peut utiliser '_' pour designer la memoire
  //    partagee, par exemple : 
  //        MUTEX @ _ : P
  //        STORE @ _$2 : alpha
  //        MUTEX @ _ : V 
  //
  //  Instruction complexe 
  //
  //    . WHILE  @ <nombre> (<nbrEntOuNomVar> <oper> <nbrEntOuNomVar>) REPEAT 
  //          <instruction>
  //          <instruction>
  //          ...
  //          <instruction>
  //      ENDWHILE @ <memeNombreQueCeluiDeCeWHILE>
  //
  // le nombre qui etiquette un WHILE sert non seulement a aider au
  // parsing mais aussi a mieux se reperer dans le programme
  // 
  // le 'if' est un WHILE dont la condition devient fausse avant la premiere
  // arrivee a son ENDWHILE. le 'else' d'un if est un WHILE qui suit, avec
  // la condition inversee ET une variable vraie si on n'est PAS DU TOUT
  // rentre dans le premier WHILE (celui du if)
  // les <oper>ateurs etant +,-,*,/,%,>,<,>=,<=,==,!= (comme en C/C++).
  // et les calculs etant tous faits en nombres entiers.
  // il y a egalement $ pour le []: a$2 <=> a[2].
  // il faut remarquer (entre autres) qu'il ne peut pas y avoir de
  // variable non-initialisee, car la "declaration" NEW se fait toujours avec
  // une valeur, etant une definition, appelle 'creation'
  // par contre, la zone memoire "tas" (heapMemory) ou l'on accede avec
  // l'operateur $ n'est PAS INITIALISEE ! De meme, il n'y a pas 
  // d'operateur '&' (adresse d'une variable). L'unite de la heapMemory
  // est l'entier (et non pas l'octet). 


  
  // il faut remarquer, du point de vue de la syntaxe, que le seul endroit
  // ou les parantheses apparaissent est dans le WHILE. Un COMPUTE n'a qu'un
  // seul operateur et exactement un, qui est binaire, prenant deux operandes.


  // les programme/processus a emuler sont geres par les structures 
  // qui suivent, et, attention, leur simulation  n'est pas faite en 
  // faisant un fork() Unix reel pour chacun. leur avancement est 
  // simule pas-a-pas directement en faisant avancer des indices 
  // et en changeant des valeurs de donnees-membres, de maniere similaire
  // a ce qui a ete vu en TP (mais bien entendu plus complexe). 

  class ProcInfo {
    // megastructure avec le tout ce qui faut
    // un seul objet en sera cree a l'utilisation

    // contient entre autres un vector de ProcData's
    // qui sont les objets par programme/processus

    // chaque ProcData contient divers renseignements 
    // (initialisees, constants ou evoluant, comme des 
    // accumulateurs de temps, l'etat du processus, etc.) et aussi
    // un arbre de noeuds ProcInstruction *, nomme proGram

    // tres important, chaque ProcInstruction a bien entendu un 
    // programCounter, qui est l'indice dans son bodyInstr
    // donnant l'instruction EN COURS a son niveau

    // on parlera de pid de programme/processus a simuler, 
    // et ce pid est un simple entier, indice dans le
    // vector de ProcData's

    // ce pid ne doit pas etre confondu avec le pid Unix reel,
    // qui n'intervient pas du tout


  public:    
    enum ProcStatus {
        STAT_WAITING, STAT_RUNNING, 
        STAT_IOWAIT, 
        STAT_MUTEXWAIT, STAT_MUTEXGRAB, STAT_NOMUTEX,
        STAT_SYS, STAT_TERMINATED,
        STAT_TRACEEND, STAT_TRACESTEPRUN
    };
    enum ProcInfoOperType { // c'est pour demander a
      // la fonction
      // updateProcData(), d'avancer pas-a-pas
      // chaque processus en cours de simulation
      ADVANCE_PROC, 
      PROC_TERM,
      START_TRACE,
      END_TRACE,
    };
    
  private:
    enum ProcInstructionType {
        DO_NEW, 
        DO_COMP, DO_COPY, DO_LOAD, DO_STORE,
        DO_READ, DO_PRINT, 
        DO_FORK, DO_MUTEX,
        DO_WHILEREPEAT, DO_PROGRAM, DO_NOTHING,
        DO_SIGNAL, DO_SIGADD, DO_SIGDEL
    };
    enum ProcAdvanceType {
        ADV_ONE_MORE_STEP_INSIDE, ADV_REACHED_END,
        ADV_REACHED_END_AND_NOW_DUPLICATED
    };
    enum ProcCondEval {
        COND_NOT_EVAL, COND_EVAL_TRUE, COND_EVAL_FALSE
    };
    enum ProcOperandType {
        OPND_TYPE_INT, OPND_TYPE_STR
    };
    enum ProcOperType {
        OP_NOP,OP_INSTR,OP_ENUM,OP_ASSIGN,
        OP_STARTWHILEARG,OP_ENDWHILEARG,
        OP_STARTSTRING, OP_ENDSTRING, 
        OP_MEMINDEX,
        OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_REM, 
        OP_GT, OP_LT, 
        OP_GE, OP_LE, OP_EQ, OP_DIFF
    };
    enum InstrTokenType {
        INSTRTOK_STRING, INSTRTOK_NUMBER, 
        INSTRTOK_SYMBOL, INSTRTOK_SPECIAL,
        INSTRTOK_OPER
    };
    struct InstrToken {
        std::string token;
        InstrTokenType tokenType;
        ProcOperType tokenOperType;
        InstrToken(const std::string &, InstrTokenType, bool, 
                   ProcOperType tOpT = OP_NOP);
    };
    typedef std::vector<std::deque<InstrToken> > ProgToken;

    struct ProcSymbol {
        bool   qIsConst;
        std::string varIdent;
        int    value;
        std::string strValue;
        ProcOperandType opType;
        ProcSymbol(const std::string &varId, int intVal, 
                   const std::string &strVal = "", 
                   ProcOperandType opT = OPND_TYPE_INT,
                   bool qCst = false);
    };

    struct ProcInstruction { // pour chaque instruction
        ProcInstructionType           instructionType;
        ProcOperType                  operType; // pour DO_{COMP,WHILE}
        // les deux donnees-membres suivantes sont des indices dans
        // la "table des symboles" du processus
        std::vector<int>              operand;  // autant qu'il en faut
        int                           leftValue;// si besoin
        std::vector<ProcInstruction*> bodyInstr;// pour DO_WHILEREPEAT
                                                // et DO_SIGNAL
        ProcCondEval                  condEval; // pour DO_WHILEREPEAT
        int                           programCounter; // index dans bodyInstr
        ProcInstruction *             father;   //DO_{WHILEREPEAT,PROGRAM}englb
        int                           lineNumber;//dans le fichier source
        std::string                   fileName;
        ProcInstruction (ProcInstructionType t = DO_NOTHING,
                         ProcOperType opT = OP_NOP, 
                         const std::vector<int> &opNd = std::vector<int>());
        ProcInstruction (const ProcInstruction &);
    }; // seront mises dans l'arbre proGram
    
    struct ProcData { // pour chaque programme/processus a simuler
        std::string                    progName; // nom du fichier
        // les deux composantes essentielles
        ProcInstruction              * proGram; // aura le type DO_PROGRAM
        // et son "father" sera nul
        ProcInstruction              * hanDler; // ici seront mises
        // les instructions entre SIGNAL et ENDSIGNAL
        sigset_t                       sigMask; // masque des signaux
        // a "derouter" vers SIGNAL ... ENDSIGNAL
        ProcInstruction              * proGramInit; // copie de proGram quand
        // il est initialisé dans le constructeur de ProcInfo
        std::vector<ProcSymbol>        symbolTable;
        // car toutes les variables sont globales, pour simplifier
        int                            lastAnonym; // pour les
        // noms symboliques des constantes
        // et une association, qui servira egalement pour le debugger
        typedef std::map<std::string,int> Name2ProcSymbolIndex;

        Name2ProcSymbolIndex name2ProcSymbolIndex;
        std::vector<int>             heapMemory; // pour les a$2 LOAD/STORE
        int                          heapMemoryLimit;
        // les autres donnees-membres essentielles pour l'execution
        ProcStatus   procStatus,procMutexStatus;
        int          nextLineNumber;
        // le constructeur et les methodes
        ProcData                  (const std::string &name = "<Anonymous>",
                                   int memL = 10000) ;
        ProcData                  (const ProcData &);
        ProcInstruction *parseProg(const ProgToken &fileContent,
                                   unsigned int firstLine,
                                   unsigned int lastLine,
                                   bool qParsingVerbose = false,
                                   unsigned int *newLastLine = 0); 
        ProcInstruction *parseSignal(const ProgToken &fileContent,
                                   unsigned int firstLine,
                                   unsigned int lastLine,
                                   bool qParsingVerbose = false,
                                   unsigned int *newLastLine = 0); 
        // pour empiler chaque fois un nouvel objet ProcInstruction 
        // dans l'arbre proGram, au bon endroit
        int findExistentSymbol    (const std::string&);
        int addNewSymbol          (const InstrToken &);
    };

    // cette methode est appelee par updateProcData(), qui commande
    // l'avancement; doOneStepAndAdvancePC() avance les program counters
    // et appelle doTheInstruction() ou doTheExpressionOfThe() pour faire
    // faire effectivement le travail
  
    ProcAdvanceType   doOneStepAndAdvancePC(const int ,
                                            ProcInstruction *,
                                            int *              nwPid = 0,
                                            ProcInstruction ** frkInstr = 0);

    // ces deux methodes font donc effectivement le travail 

    bool doTheInstruction                  (const int,
                                            ProcInstruction *,
                                            int *             nwPid = 0,
                                            ProcInstruction ** frkInstr = 0);
    int               doTheExpressionOfThe (const int, 
                                             ProcInstruction *,
                                             bool *);   
    // puisqu'on a simplifie, et on a au plus une expression par 
    // instruction
    void              doTerminateProc      (const int);
    
  public:
    
    std::vector<ProcData *> procData;           // indexe par les pids des 
    // programmes/processus a simuler
    int                 outstandingProcCount; // decremente au fur et a
    // mesure que les processus terminent
    static const int ONE_INSTRUCTION_SLEEP = 1;
    static const int memoryLimitForAll  = 10000;
    static const int THE_SHARED_MEMORY = -2;//au lieu d'indices ds symbolTable
    static const int THE_MUTEX         = -3;//au lieu d'indices ds symbolTable 
    static const int MUTEX_OPER_P      = -1;
    static const int MUTEX_OPER_V      = -2;
    static const char * procStateStr[];
    static const char * operChar[];
    static const char * instructionKeyword[];
    static const std::string inlineRegOper;
    static const int    invalidProcPid = -1;
    int                 mutex;     
    std::vector<int>    sharedMemory;
    int                 sharedMemoryBase;
    int                 sharedMemoryLimit;

    Scheduler          *scheduler;

    ProcInfo(const std::string &, 
             bool qMnSV = false, bool qTokV = false, 
             bool qPrsV = false, bool qExecV = false); 
    bool  tokenizeInstr(const std::string &, std::deque<InstrToken> &);
    ProcInstruction *findCrtInstruction(ProcInstruction *crtInstr);
    void dumpInstruction(std::ostream *s, const std::string &, 
                         ProcInstruction *crtInstr);
    bool  qDoSleepAfterEachInstruction; 
    bool  qMainStepsVerbose;
    bool  qTokenizingVerbose;
    bool  qParsingVerbose;
    bool  qExecutingVerbose;
    void   displayProcInfo   (std::ostream *, const int, bool qDump = false);
    void   dumpProcInfoStat  (std::ostream *)    const;
    std::string getProcName  (const int procPid) const;
    int    updateProcData    (const int procPid, 
                              const ProcInfoOperType  p,
                              ProcInstruction * progAAvancer = NULL);
    // le dernier argument est utilisé pour avancer le programme
    // dans le traitant (SIGNAL ... ENDSIGNAL)

    // fonction rajoutée
    void       avancerDUnPas (const int procPid,
                              ProcInstruction * progAAvancer = NULL);

  }; // class ProcInfo
  
  class Scheduler {
  public:
    Scheduler(ProcInfo   *pI  = 0, bool qSchV = false);

  private:
    ProcInfo   *pInfo;
    std::deque<int>               waitQueue;
    std::vector<std::deque<int> > mLevelQueue;
  public:
    void enQueueProc   (const int procPid);
    void enQueueAllProc();
    void displayQueue  (std::ostream *s);
    bool qSchedulingVerbose;
    
    int electAProc     ();

  };


    inline ProcInfo::ProcSymbol::ProcSymbol
    (const std::string &varId, int intVal, 
     const std::string &strVal /* = ""*/, 
     ProcOperandType opT  /* = OPND_TYPE_INT*/,
     bool qCst /*= false*/) : qIsConst (qCst),varIdent(varId),
                              value(intVal), strValue(strVal),
                              opType(opT) {}

    inline ProcInfo::ProcInstruction::ProcInstruction 
    (ProcInfo::ProcInstructionType t /* = DO_NOTHING */, 
     ProcOperType opT /* = OP_NOP */, 
     const std::vector<int> & opNd /* = just the empty one */) :  
        instructionType (t), operType (opT), operand (opNd), 
        leftValue (-1), condEval(COND_NOT_EVAL), programCounter (-1),
        father(0), lineNumber(-1)
    {}
    
    inline ProcInfo::ProcData::ProcData(const std::string &name, int memL):
        progName           (name), 
        proGram            (0),
        hanDler            (0),
        proGramInit        (0),
        lastAnonym         (0),
        heapMemoryLimit    (memL),
        procStatus         (STAT_WAITING), 
        procMutexStatus    (STAT_NOMUTEX),
        nextLineNumber     (1) {
        heapMemory . resize(heapMemoryLimit); // on pourrait optimiser, en 
        // retardant ceci, pour le faire graduellement dans
        // doTheInstruction(), lors d'un STORE qui depasserait... enfin bref.
    }
    
    inline Scheduler::Scheduler(ProcInfo   *pI /* = 0*/,
                                bool qSchV  /* = false*/) :
        pInfo (pI), qSchedulingVerbose(qSchV) {}    
}

#endif /*  __PROCDEBUG_H__ */
