/**
 *
 * @File : ProcDebug.cxx
 *
 * @Author : A. B. Dragut
 *
 * @Date : 14/10/2006
 *
 * @Version : V1.0
 *
 * @Synopsis : debugger-emulateur
 *
 **/

#include <iostream> 
#include <fstream> 
#include <string>
#include <vector>
#include <deque> 
#include <sstream>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

#include "ProcDebug.h"
#include "nsSysteme.h"

using namespace std;
using namespace nsSysteme;

#define STATUS procData[procPid] -> procStatus

namespace ProcDebug {

    // Une sorte de wrapper de updateProcData (procPid, ADVANCE_PROC)
    // On change l'état avant et après l'exécution de l'instruction
    // (TRACESTEPRUN > TRACEEND ou RUNNING > WAITING)
    // selon si le processus est tracé ou non
    // et on garanti la granularité du débugger instructions par
    // instructions en bloquant les signaux dans sigMask qui contient
    // au début SIGQUIT, et on peux en rajouter avec SIGADD (mini-langage)

    void ProcInfo::avancerDUnPas (const int procPid,
                                  ProcInfo::ProcInstruction * prog
                                                            /* = NULL */ ) {

        if (NULL == prog) prog = procData[procPid] -> proGram;

        // normalement inutile puisqu'on le vérifie avant d'appeler
        // avancerDUnPas(), mais on ne sait jamais...
        if (procData[procPid] -> procStatus == STAT_TERMINATED) return;

        // Bloquage de SIGQUIT
        Sigprocmask (SIG_BLOCK,   &procData[procPid] -> sigMask, 0);

        // Changement de l'état du processus pour celui qui va bien...
        STATUS = (STATUS == STAT_TRACEEND) ? STAT_TRACESTEPRUN
                                           : STAT_RUNNING;
        // On avance d'une instruction
        updateProcData (procPid, ADVANCE_PROC, prog);

        if (STATUS != STAT_TERMINATED)
            STATUS = (STATUS == STAT_TRACESTEPRUN) ? STAT_TRACEEND
                                                   : STAT_WAITING;

        // Débloquage après le changement de status car
        // le débloquage peut entraîner l'exécution d'un traitant,
        // qui pourrait changer le signal, qui serait perdu au retour
        // de ce traitant...
        Sigprocmask (SIG_UNBLOCK,  &procData[procPid] -> sigMask, 0);

    } // avancerDUnPas()

    // la fonction principale pour l'avancement instruction par instruction,
    // que vous devez ameliorer (eventuellement en rajoutant d'autres
    // fonctions egalement) pour realiser le debugger pas-a-pas avec inspection
    // des variables et controle de l'execution
    
    // gardez a l'esprit que la granularite du debugger est l'instruction,
    // autrement dit, le debugger intervient ENTRE deux instructions, et il 
    // LAISSE TRANQUILLE tout le mecanisme pour EXECUTER une instruction

    int ProcInfo::updateProcData(const int procPid, 
                                 const ProcInfo::ProcInfoOperType p,
                                 ProcInfo::ProcInstruction * prog
                                                            /* = NULL */) {

        if(procPid == ProcInfo::invalidProcPid) {
            return 0;
        }
        switch(p) {
            case PROC_TERM: {
                doTerminateProc(procPid);      
                return 0;
            }
            case START_TRACE: {
                scheduler -> enQueueProc(procPid);
                if(procData[procPid] -> procStatus == STAT_TRACESTEPRUN) {
                    return 0;
                }
                procData[procPid] -> procStatus = STAT_TRACEEND;
                return 0;
            }
            case END_TRACE: {
                procData[procPid] -> procStatus = STAT_WAITING;
                scheduler -> enQueueProc(procPid);
                return 0;
            }
            case ADVANCE_PROC: {
                if(qExecutingVerbose) {
                    displayProcInfo(&cerr, procPid);
                }
                if(procData[procPid] -> procStatus != STAT_TRACEEND) {
                    doOneStepAndAdvancePC(procPid, prog);
                    if(procData[procPid] -> procStatus != STAT_TERMINATED) {
                        scheduler -> enQueueProc(procPid);
                    }
                    return 0;
                }
                scheduler -> enQueueProc(procPid);
                return 0;
            }
            default: cerr << "ERROR Unknown ProcInfoOperType " << p << "\n";
                return -1;
                break;
        }
        return -2;
    } // updateProcData()

    // fonction pour obtenir le pointeur sur l'instruction
    // courante (donc celle determinee par l'ensemble des compteurs
    // ordinaux), a partir du programme principal d'un processus
    // pour un pid donne, on peut l'appeler ainsi
    //      findCrtInstruction(procData[pid] -> proGram);
    // et bien entendu, recuperer le pointeur qu'elle envoie.
    // au cas ou.

    // attention, a tout moment ENTRE un appel d'updateProcData() 
    // et un autre, les compteurs ordinaux pointent vers la
    // PROCHAINE INSTRUCTION qui sera executee, et non pas vers
    // celle qui vient d'etre executee, donc apres la terminaison
    // ce renseignement n'a plus de sens.

    ProcInfo::ProcInstruction *ProcInfo::findCrtInstruction(
        ProcInfo::ProcInstruction *crtInstr){
        if(crtInstr == 0) return 0;
        switch(crtInstr -> instructionType) {
            case DO_WHILEREPEAT:
            case DO_PROGRAM:
                if(crtInstr -> programCounter == -1) {
                    return crtInstr;
                }
                // sinon, c'est comme pour la suite, on descend
                if(crtInstr -> programCounter > (int)(crtInstr -> 
                   bodyInstr . size()) - 1) {
                    cerr << "INTERNAL ERROR programCounter out of bounds "
                         << crtInstr -> programCounter << " in "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    exit(4);
                }
                return findCrtInstruction(crtInstr -> 
                                          bodyInstr[crtInstr -> 
                                          programCounter]);
            default: return crtInstr;
        }
    } // findCrtInstruction()

    // fonction pour afficher l'essentiel des renseignements sur
    // un processus de pid donne -- tres instructive a examiner

    void ProcInfo::displayProcInfo(ostream *s, const int procPid,
                                   bool qDump /* = false */) {
        (*s) << "\n[" << procPid << "]"
             << procData[procPid] -> progName            << ":";
        if(procData[procPid] -> procStatus == STAT_TERMINATED) {
            (*s) << "ENDPROGRAM\n"; 
            return; // car il n'y a plus rien d'autre a afficher
        }
        // sinon, on en a un paquet
        (*s) << procData[procPid] -> nextLineNumber+1 << ".";
        ProcInstruction * p (findCrtInstruction(
                             procData[procPid] -> proGram));
        // p pointe maintenant vers l'instruction QUI SERA executee
        // au prochain appel de doOneStepAndAdvancePC()  
        if(p) { // pour eviter des surprises
            if(p -> instructionType == DO_PROGRAM) { // cas special
                if(p -> programCounter == -1) { // tout au debut
                    if(p -> bodyInstr . size()) {
                        (*s) << instructionKeyword[p -> bodyInstr[0] 
                                   -> instructionType];
                    }
                    else {
                        (*s) << "EMPTY PROGRAM...\n";
                    }
               }
               else (*s) << "???"; // ca ne devrait pas arriver
                // car l'"instruction" PROGRAM n'est jamais
                // celle A EXECUTER sauf tout au debut et apres la fin,
                // (pour le cas ou on reprend), mais le cas de la fin a
                // deja ete traite plus haut, car la fin entraine 
                // automatiquement le passage dans l'etat STAT_TERMINATED.
            } // fin du cas special "debut du programme"
            else { 
                (*s) << ((p -> instructionType == DO_WHILEREPEAT && 
                          p -> condEval == COND_EVAL_FALSE)?"END":"");
                // et ceci est valable pour toutes les instructions
                (*s) << instructionKeyword[p -> instructionType];
                // ainsi que la suite des renseigments
                (*s) << "^" << p ->  lineNumber+1 << "$" 
                     << p -> programCounter ;
                if(p -> father) { // juste pour voir. ce ne passera pas
                    // pour le PROGRAMme lui-meme (car il n'a pas de pere)
                    // mais ceci sera le cas uniquement au debut ou a la fin
                    (*s) << "&" << p -> father -> programCounter;
                }
                (*s) << " @" << p -> leftValue;
                if(p -> leftValue >= 0 && 
                   p -> leftValue < (int)procData[procPid] -> 
                   symbolTable . size()){
                    (*s) << " "  
                         << procData[procPid] -> symbolTable[p -> leftValue] . 
                        varIdent << "="
                         << procData[procPid] -> symbolTable[p -> leftValue] . 
                        value;
                }
                (*s) << " :(" << operChar[p -> operType] << " ";
                for(unsigned int k = 0 ; k < p -> operand . size() ; ++k) {
                    (*s) << "@" << p -> operand[k] ;
                    if(p -> operand[k] >= 0 && 
                       p -> operand[k] < (int)procData[procPid] -> symbolTable . 
                       size()){
                        (*s) << " "  
                             << procData[procPid] -> 
                            symbolTable[p -> operand[k]] . varIdent << " = "
                             << procData[procPid] -> 
                            symbolTable[p -> operand[k]] . value;
                    }
                    (*s) <<  ((k < p -> operand . size() - 1)?", ":" ");
                }
                (*s) << ") ";
            }
        } // fin du cas heureux quand p pointe bien sur une instruction
        else {
            (*s) << "?????"; // ca ne devrait pas arriver
        }
        (*s) << "_"
             << procStateStr[procData[procPid] -> procStatus] 
             << ","
             << procStateStr[procData[procPid] -> procMutexStatus]
             << " " 
             << "\n";
        if(qDump) {
            dumpInstruction(s,"",procData[procPid] -> proGram);
        }
    } // displayProcInfo()


    //////////////////////////////////////////////////////////////////////
    // avec ces trois fonctions et les structures de donnees declarees   //
    // dans ProcDebug.h, dont notamment symbolTable, vous avez tous     // 
    // les elements requis pour realiser quasiment tout ce qui vous     //
    // est demande, tout en revisitant egalement proj.cxx, bien entendu //
    //////////////////////////////////////////////////////////////////////

    //==================================================================//

    //******************************************************************//
    // EN PRINCIPE il ne devrait pas y avoir a changer des choses       //
    //             a partir de ce point-ci, pour le gros du projet      //
    //******************************************************************//
    // Vous pouvez etudier brievement ce code, cependant son entiere 
    // comprehension n'est pas du tout requise pour la realisation de 
    // la partie essentielle du projet.  
    
    // Rappel :
    // gardez a l'esprit que la granularite du debugger est l'instruction,
    // autrement dit, le debugger intervient ENTRE deux instructions, et il 
    // LAISSE TRANQUILLE tout le mecanisme pour EXECUTER une instruction
    // C'est donc pourquoi pour le gros de votre travail vous n'avez pas du 
    // tout besoin d'assimiler en profondeur le code qui suit. Vous devez par
    // contre bien maitriser les donnees membres de ProcInfo (ProcDebug.h)
    
    // SOMMAIRE de ce qui suit: 
    //
    // constructeurs (totalement orthogonaux au gros du travail demande)
    // fonctions de parsing (totalement orthogonales au travail demande)
    // fonctions pour l'execution pas a pas (peut-etre un coup d'oeil rapide)
    // fonctions pour l'ordonnancement (tourniquet seulement -- vu en TP)
    
    // Le code qui suit est neanmoins assez commente par endroits, 
    // pour ceux qui auront tout fini et voudront ameliorer des choses...

    const int    ProcInfo::ONE_INSTRUCTION_SLEEP;
    const int    ProcInfo::THE_SHARED_MEMORY;
    const int    ProcInfo::THE_MUTEX;
    const int    ProcInfo::MUTEX_OPER_P;
    const int    ProcInfo::MUTEX_OPER_V;
    const char * ProcInfo::procStateStr[] = {"wait","run",
                                             "io",
                                             "mutexwait","mutexgrab","nomutex",
                                             "sys","term",
                                             "tracestop","tracerun"};
    const char  * ProcInfo::operChar[]    = {" ","@",",",":",
                                             "(",")","\"","\"",
                                             "$",
                                             "+","-","*","/","%",
                                             ">","<",
                                             ">=","<=","==","!="};
    const string ProcInfo::inlineRegOper  = 
    operChar[ProcInfo::OP_INSTR]+ string(operChar[ProcInfo::OP_ENUM])
        + operChar[ProcInfo::OP_ASSIGN] 
    + operChar[ProcInfo::OP_MEMINDEX]
    + operChar[ProcInfo::OP_ADD]    
    + operChar[ProcInfo::OP_SUB]
    + operChar[ProcInfo::OP_MUL]    
    + operChar[ProcInfo::OP_DIV]
    + operChar[ProcInfo::OP_REM]    
    + operChar[ProcInfo::OP_GT]     
    + operChar[ProcInfo::OP_LT]
    + operChar[ProcInfo::OP_EQ][0]  
    + operChar[ProcInfo::OP_DIFF][0];

    
    const char  * ProcInfo::instructionKeyword[] = {"NEW","COMPUTE",
                                                    "COPY","LOAD","STORE",
                                                    "READ","PRINT","FORK",
                                                    "MUTEX","WHILE","PROGRAM",
                                                    "NOTHING", "SIGNAL",
                                                    "SIGADD", "SIGDEL"};
    
    ProcInfo::ProcInstruction::ProcInstruction(const ProcInstruction& instr) {
        // le constructor par recopie -- tres important pour le FORK
        // il doit faire new pour chaque noeud de l'arbre, pour dupliquer
        // reellement l'ensemble des instructions
        instructionType = instr . instructionType;
        operType        = instr . operType;
        operand         = instr . operand;
        leftValue       = instr . leftValue;
        bodyInstr . resize(instr . bodyInstr . size());
        for(unsigned int k = 0; k < instr . bodyInstr . size() ; ++k) {
            bodyInstr[k] = new ProcInstruction(*( instr . bodyInstr[k]));
            bodyInstr[k] -> father = this;
        }
        condEval        = instr . condEval;
        programCounter  = instr . programCounter;
        father          = 0; //sera normlmnt initialise par celui qui appelle
        lineNumber      = instr . lineNumber;
        fileName        = instr . fileName;
    }
    ProcInfo::ProcData::ProcData(const ProcData& pData) {
        // le constructor par recopie -- tres important pour le FORK
        // il fait usage du constructeur par recopie de ProcInstruction
        // reellement l'ensemble des instructions
        if(pData . proGram) {
            proGram          = new ProcInstruction(*(pData . proGram));
        }
        else {
            proGram          = 0;
        }
        if (pData . hanDler) {
            hanDler          = new ProcInstruction(*(pData . hanDler));
        }
        else {
            hanDler          = 0;
        }
        if (pData . proGramInit) {
            proGramInit      = new ProcInstruction(*(pData . proGramInit));
        }
        else {
            proGramInit      = 0;
        }
        progName             = pData . progName;
        sigMask              = pData . sigMask;
        symbolTable          = pData . symbolTable;
        lastAnonym           = pData . lastAnonym;
        name2ProcSymbolIndex = pData . name2ProcSymbolIndex;
        heapMemory           = pData . heapMemory;
        heapMemoryLimit      = pData . heapMemoryLimit;
        procStatus           = pData . procStatus;
        procMutexStatus      = pData . procMutexStatus;
        nextLineNumber       = pData . nextLineNumber;
    }

    void 
    ProcInfo::dumpInstruction(ostream *s, const string &indent,
                              ProcInfo::ProcInstruction *crtInstr){
        if(crtInstr == 0) return;
        (*s) << indent << " file ";
        (*s) << crtInstr -> fileName << ":"
             << crtInstr -> lineNumber + 1 << " "
             << instructionKeyword[crtInstr -> instructionType] << " @ "
             << crtInstr -> programCounter
             << "\n";
        switch(crtInstr -> instructionType) {
            case DO_WHILEREPEAT:
            case DO_PROGRAM:
                for(unsigned int k = 0; k < crtInstr -> bodyInstr . size();
                    ++k) {
                    dumpInstruction(s, indent + "  ", 
                                crtInstr -> bodyInstr[k]);
                }
                (*s) << indent << " file ";
                (*s) << crtInstr -> fileName << ":"
                     << crtInstr -> lineNumber + 1 << " END"
                     << instructionKeyword[crtInstr -> instructionType] 
                     << "\n";             
            default: return;
        }
    }
    // quelques fonctions auxiliaires pour le parsing
    
    void stripWhiteSpace(string &str) {
        string::size_type firstNonBlanc = 
            str . find_first_not_of(" ");
        if(firstNonBlanc == string::npos) return;
        string::size_type lastNonBlanc = 
            str . find_last_not_of(" ");
        str = str . substr(firstNonBlanc, 
                           lastNonBlanc - firstNonBlanc + 1);
    }

    bool processEscapeSequences(string &str) {
        for(string::size_type kPos (0) ; kPos < str . size() &&
            (kPos = str . find("\\",kPos))!= string::npos;) {
            string::size_type theEscapedChrPos(kPos + 1);
            if(theEscapedChrPos >= str . size()) {
                cerr << "SYNTAX ERROR Missing chr after '\\' "
                     << "(add one for '\\' itself) " 
                     << str << " pos " << kPos << "\n";
                return true;
            }
            switch(str[theEscapedChrPos]) {
                case 'n': str[kPos] = '\n';break;
                case 'r': str[kPos] = '\r';break;
                case 't': str[kPos] = '\t';break;
                case '\'': str[kPos] = '\'';break;
                case '"': str[kPos] = '"';break;
                case '\\': str[kPos] = '\\';break;
                default: cerr << "SYNTAX ERROR Unknown escape sequence "
                              << "\\" << str[theEscapedChrPos] 
                              << str << " pos " << kPos << "\n";
                    return true;
            }
            str . erase(theEscapedChrPos,1);
            kPos ++;
        }
        return false;
    } // processEscapeSequences()
    
    void toUpper(string &t) {
        for(unsigned int k = 0 ; k < t . size(); ++k) {
            t[k] = toupper(t[k]);
        }
    }

    ProcInfo::InstrToken::InstrToken(const string &s,
                                     InstrTokenType t,
                                     bool qDoStripWhiteS,
                                     ProcOperType tOpT /*= OP_NOP*/) 
        : token (s), tokenType(t), tokenOperType(tOpT) {
        if(qDoStripWhiteS) stripWhiteSpace(token);
        if(processEscapeSequences(token)) {
            exit(1);
        }
        if(tokenType == INSTRTOK_SYMBOL) { // on verifie quand meme
            for(unsigned int k = 0; k < s . size(); ++k) {
                if((s[k] >= 'a' &&  s[k] <= 'z') ||
                   (s[k] >= 'A' &&  s[k] <= 'Z')) continue;
                cerr << "SYNTAX ERROR " << s << " pretends to be "
                     << " a symbol, but it has a non alphabetical "
                     << " character " << s[k] << " in it.\n";
                exit(2);
            }
        }
    }

    int ProcInfo::ProcData::findExistentSymbol(const string &name) {
        Name2ProcSymbolIndex::const_iterator 
            tokIter(name2ProcSymbolIndex . 
                    find(name));
        if(tokIter != name2ProcSymbolIndex . end()) {
            return tokIter -> second;
        }
        return -1; // pas trouve
    }

    int ProcInfo::ProcData::addNewSymbol(const InstrToken &token) {
        ProcOperandType theType (OPND_TYPE_STR);
        int intVal(0);
        switch(token . tokenType) {
            case INSTRTOK_SYMBOL: { // on sait que c'est un int
                symbolTable . push_back(ProcSymbol(token . token, 0));
                return((name2ProcSymbolIndex[token . token] = 
                        symbolTable . size() - 1));
            }
            case INSTRTOK_NUMBER: theType = OPND_TYPE_INT;
                intVal = atoi(token . token . c_str());
            case INSTRTOK_STRING: {
                ostringstream buffStr;
                buffStr << "AnnymSym" << lastAnonym;
                const string newId (buffStr . str());
                symbolTable . push_back(
                    ProcSymbol(newId, intVal, token . token, theType));
                return((name2ProcSymbolIndex[token . token] = 
                        symbolTable . size() - 1));
            }
            default: cerr << "ERROR Invalid token type " 
                          << token . tokenType 
                          << " in ProcInfo::addNewSymbol() for "
                          << token . token << "\n";
                return -1;
        }
    }

    // fonction importante pour le parsing : decoupage d'une ligne en
    // lexemes (tokens) (la deque est la pour la facilite d'insertion) et
    // leur categorisation (symbole, nombre, operateur (et lequel), etc.)

    // elle tire profit de la syntaxe volontairement simple, qui 
    // n'imbrique pas les parantheses, etc. parmi les rares "goodies", 
    // on peut noter les quelques sequences d'echappement disponibles
 
    bool ProcInfo::tokenizeInstr(
        const string & line, deque<InstrToken> & token) {
        // separateurs (priorite) : [" "] [( )] [,] [<oper>]
        if(qTokenizingVerbose) {
            cerr << "Tokenizing " << line << "\n";
        }
        string::size_type guillPos = 0;
        if((guillPos = line . find(operChar[OP_STARTSTRING])) 
           != string::npos) {
            string::size_type matchingGuillPos(guillPos);
            do{
                matchingGuillPos = 
                    line . find(operChar[OP_ENDSTRING],matchingGuillPos+1);
                if(string::npos == matchingGuillPos) {
                    cerr << "SYNTAX ERROR Missing endstring " 
                         << operChar[OP_ENDSTRING]
                         << "...\n";
                    return true;
                }
            }
            while(line[matchingGuillPos - 1] == '\\');
            token . push_back(InstrToken(line . substr(guillPos + 1, 
                                         matchingGuillPos - 
                                         guillPos - 1),
                              INSTRTOK_STRING, false));
            deque<InstrToken> token1, token2;
            string firstPart (line . substr(0,guillPos));
            stripWhiteSpace(firstPart);
            bool a = tokenizeInstr(firstPart, 
                                   token1);
            string secondPart (line . substr(matchingGuillPos + 1));
            stripWhiteSpace(secondPart);
            bool b = tokenizeInstr(secondPart,
                                   token2);
            for(int k = token1 . size() - 1; k >= 0; --k) {
                token . push_front(token1[k]);
            }
            for(unsigned int k = 0 ; k < token2 . size() ;  ++k) {
                token . push_back(token2[k]);
            }
            return a && b;
        } // if(guillemets)
        string::size_type parenPos;
        if((parenPos = line . find(operChar[OP_STARTWHILEARG])) 
           != string::npos) {
            string::size_type matchingParenPos = 
                line . find(operChar[OP_ENDWHILEARG],parenPos+1);
            if(string::npos == matchingParenPos) {
                cerr << "SYNTAX ERROR Missing " 
                     << operChar[OP_ENDWHILEARG] << " ...\n";
                return true;
            }
            if(parenPos + 1 == matchingParenPos) {
                cerr << "SYNTAX ERROR Nothing between "
                     << operChar[OP_STARTWHILEARG] << " and " 
                     << operChar[OP_ENDWHILEARG]
                     << "...\n";
                return true;
            }
            const string::size_type whileArgLength (matchingParenPos - 
                                                    parenPos - 1);
            deque<InstrToken> token0, token1, token2;
            string whileArgPart (line . substr(parenPos+1,whileArgLength));
            stripWhiteSpace(whileArgPart);
            bool w = tokenizeInstr(whileArgPart, token0);
            string firstPart (line . substr(0,parenPos));
            stripWhiteSpace(firstPart);
            bool a = tokenizeInstr(firstPart,token1);
            string secondPart (line . substr(matchingParenPos + 1));
            stripWhiteSpace(secondPart);
            bool b = tokenizeInstr(secondPart,token2);
            for(int k = token1 . size() - 1; k >= 0; --k) {
                token . push_front(token1[k]);
            }
            token . push_back(InstrToken(operChar[OP_STARTWHILEARG],
                                         INSTRTOK_OPER,false,
                                         OP_STARTWHILEARG));
            for(unsigned int k = 0 ; k < token0 . size() ;  ++k) {
                token . push_back(token0[k]);
            }
            token . push_back(InstrToken(operChar[OP_ENDWHILEARG],
                                         INSTRTOK_OPER,false,
                                         OP_ENDWHILEARG));   
            for(unsigned int k = 0 ; k < token2 . size() ;  ++k) {
                token . push_back(token2[k]);
            }
            return a && b && w;
        } // if(parentheses, i.e. whilearg delimiters -- start and end)
        string::size_type sepPos     (0);
        string::size_type prevSepPos (0);
        for(bool qLoop (true); qLoop && prevSepPos < line . size(); ) {
            int opLength(1);
            string::size_type crtTokLength (string::npos);
            ProcOperType theOperType (OP_NOP);
            if(string::npos != (sepPos = line .
                                find_first_of(inlineRegOper,prevSepPos))) {
                                // c'est-a-dire "@:,$+-*/%<>=!"
                if(line[sepPos] == operChar[OP_LT][0] || 
                   line[sepPos] == operChar[OP_GT][0]) {
                    theOperType = OP_LT;
                    if(line[sepPos] == operChar[OP_GT][0]) {
                        theOperType = OP_GT;
                    }
                    if(sepPos + 1 ==  line . size()) {
                        cerr << "SYNTAX ERROR Missing second operand after "
                             << line[sepPos] << " ...\n";
                        return true;
                    }
                    if(sepPos + 1 < line . size() && 
                       line[sepPos+1] == operChar[OP_LE][1]) {
                        opLength++;
                        switch(theOperType) {
                            case OP_LT: theOperType = OP_LE;break;
                            case OP_GT: theOperType = OP_GE;break;
                            default: 
                                cerr << "INTERNAL ERROR bad oper type, "
                                     << "would expect OP_{L,G}T...\n"; 
                                exit(2);
                        }
                    }
                }
                else if(line[sepPos] == operChar[OP_EQ][0] || 
                        line[sepPos] == operChar[OP_DIFF][0]) {
                    if(sepPos + 1 ==  line . size() || 
                       line[sepPos+1] != operChar[OP_EQ][1]) {
                        cerr << "SYNTAX ERROR Incorrect operator " 
                             << line[sepPos]
                             << " missing " << operChar[OP_EQ][1]
                             << " after it...\n";
                        return true;
                    }
                    theOperType = OP_EQ;
                    if(line[sepPos] == operChar[OP_DIFF][0]) {
                        theOperType = OP_DIFF;
                    }
                    opLength++;
                }
                else {
                    if(line[sepPos] == operChar[OP_ADD][0]) {
                        theOperType = OP_ADD;
                    }
                    else if(line[sepPos] == operChar[OP_SUB][0]) {
                        theOperType = OP_SUB;
                    }
                    else if(line[sepPos] == operChar[OP_MUL][0]) {
                        theOperType = OP_MUL;
                    } 
                    else if(line[sepPos] == operChar[OP_DIV][0]) {
                        theOperType = OP_DIV;
                    }
                    else if(line[sepPos] == operChar[OP_REM][0]) {
                        theOperType = OP_REM;
                    }
                    else if(line[sepPos] == operChar[OP_MEMINDEX][0]) { 
                        theOperType = OP_MEMINDEX; // '$'
                    }
                    else if(line[sepPos] == operChar[OP_INSTR][0]) { 
                        theOperType = OP_INSTR;    // '@'
                    }
                    else if(line[sepPos] == operChar[OP_ENUM][0]) { 
                        theOperType = OP_ENUM;     // ','
                    }
                    else if(line[sepPos] == operChar[OP_ASSIGN][0]) {
                            theOperType = OP_ASSIGN; // ':'
                    }
                    else {
                        cerr << "INTERNAL ERROR Unexpected oper chr "
                             << line[sepPos] << "\n";
                        exit(3);
                    }
                }
                crtTokLength = sepPos - prevSepPos;
            }// if(trouve un oper)
            if(prevSepPos < sepPos) {
                string thisToken (line . substr(prevSepPos,crtTokLength));
                stripWhiteSpace(thisToken);
                if(thisToken . size() > 0) {
                    const InstrTokenType tokType(isdigit(thisToken[0])?
                                                 INSTRTOK_NUMBER:
                                                 (thisToken[0] == '_'?
                                                  INSTRTOK_SPECIAL:
                                                  INSTRTOK_SYMBOL));
                    token . push_back(InstrToken(thisToken,tokType,true));
                }
            }
            if(string::npos != sepPos) {
                token . push_back(InstrToken(line . substr(sepPos, opLength),
                                             INSTRTOK_OPER,true,theOperType));
                prevSepPos = sepPos + opLength ;
            }
            else {
                qLoop = false;
            }
        }//for(qLoop)
        return false; // tout va bien
    }// tokenizeInstr()

    // fonction principale d'analyse lexicale (parsing) d'un programme source
    // s'appuyant sur tokenizeInstr pour la preparation des lexemes
    // cree une instruction (simple ou complexe) et s'appelle recursivement
    // pour la remplir (si complexe). rappel: le programme est lui-aussi
    // une "seule" instruction, bien entendu complexe (i.e. avec un corps)

    // cette fonction renvoie zero s'il y a des erreurs, et alors on 
    // "laisse tout tomber"

    ProcInfo::ProcInstruction *
    ProcInfo::ProcData::parseProg(
        const ProgToken    &fileContent,
        const unsigned int firstLine,
        const unsigned int lastLine,
        bool  qParsingVerbose /* = false*/,
        unsigned int *newLastLine /* = 0 */) {

        // rappel de la syntaxe du minilangage
        // (les espaces blancs ne comptent pas du tout)
        // 
         // PROGRAM 
        // ... instructions ...
        // ENDPROGRAM
        //
        // le programme peut etre vu comme une seule instruction complexe
        // (i.e. avec un corps avec d'autres instructions)
        // 
        // chaque programme a simuler comporte donc des instructions
        // (une seule instruction simple par ligne, toutes en majuscules)
        // de deux types : instructions simples et instructions complexes
        //
        //  Instructions simples 

        //    . NEW     @ <Variable>       : <nbrOuAutreVariableDejaDefinie>
        //    . READ    @ <Variable>
        //    . COMPUTE @ <Variable>       : <nbrOuVar> <oper> <nbrOuVar>
        //    . STORE   @ <Var>$<nbrOuVar> : <nbrOuVar> 
        //    . LOAD    @ <nbrOuVar>       : <Var>$<nbrOuVar> 
        //    . PRINT   @ <nbrOuVarOuString>,<nbrOuVarOuString>,...
        //    . FORK    @ <Var>     // 0 si fils,pid du fils (int) si pere
        //    . MUTEX   @ _ : P            
        //    . MUTEX   @ _ : V
        //
        //   Pour le STORE ou le LOAD on peut utiliser '_' pour designer 
        //     la memoire partagee, par exemple :
        // 
        //        MUTEX @ _ : P
        //        STORE @ _$2 : alpha
        //        MUTEX @ _ : V 

        //  Une instruction complexe
        //
        //   WHILE @ <nombre> (<nbrEntOuNomVar> <oper> <nbrEntOuNomVar>) REPEAT
        //          <instruction>
        //          <instruction>
        //          ...
        //          <instruction>
        //   ENDWHILE @ <memeNombreQueCeluiDeCeWHILE>
        //
        // le nombre qui etiquette un WHILE sert non seulement a aider au
        // parsing mais aussi a mieux se reperer dans le programme
        //
        // les <oper>ateurs etant +,-,*,/,%,>,<,>=,<=,==,!= (comme en C/C++).
        // et les calculs etant tous faits en nombres entiers.
        // pour la negation, on fait a = (a == 0),
        // il y a egalement l'operateur $ pour le []: a$2 <=> a[2].
        //
        // il faut remarquer (entre autres) qu'il ne peut pas y avoir de
        // variable non-initialisee, car la "declaration" NEW se fait 
        // toujours avec une valeur, etant une definition, appellee 'creation'
        // par contre, la zone memoire "tas" (heapMemory) ou l'on accede avec
        // l'operateur $ n'est PAS INITIALISEE ! De meme, il n'y a pas 
        // d'operateur '&' (adresse d'une variable). L'unite de la heapMemory
        // est l'entier (et non pas l'octet), et on en a "plein" de disponible

        // les variables sont toutes GLOBALES, connues A PARTIR de leur NEW.
        if(qParsingVerbose) {
            cerr << "\nParsing " << progName << ":" <<  firstLine+1 << "...";
        }
        ProcInstruction *newInstr = new ProcInstruction;
        newInstr -> lineNumber    = firstLine;
        newInstr -> fileName      = progName;
        if(fileContent[firstLine][0] . token == "PROGRAM") {
            newInstr -> instructionType = DO_PROGRAM;
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[newInstr -> 
                                                     instructionType];
            }
            for(unsigned int k = lastLine ; k > firstLine; --k) {
                if(fileContent[k][0] . token == "ENDPROGRAM") {
                    unsigned int theNewLastLine   = k - 2;
                    int runningFirstLine = firstLine + 1;
                    while(theNewLastLine < k - 1) {
                        newInstr -> bodyInstr . push_back(
                            parseProg(fileContent,
                                      runningFirstLine,
                                      k - 1,qParsingVerbose,
                                      &theNewLastLine));
                        if(0 == newInstr -> bodyInstr . back()) {
                            delete newInstr;
                            return 0;
                        }
                        runningFirstLine = theNewLastLine + 1;
                        newInstr -> bodyInstr . back() -> father = newInstr;
                    }
                    if(newLastLine) {
                        (*newLastLine) = k;
                    } 
                    if(qParsingVerbose) {
                        cerr << "\nParsing " << progName << ":" 
                             <<  k << "...";
                        cerr << " -> END" << instructionKeyword[
                            newInstr -> instructionType] 
                             << " ";
                    }    
                    return newInstr;
                }
            }
            cerr << "SYNTAX ERROR Missing 'ENDPROGRAM' to match 'PROGRAM'\n";
            delete newInstr;
            return 0;
        }
        // parce que ça ne marche pas comme PROGRAM...
        if (fileContent[firstLine][0] . token == "ENDSIGNAL") {
            cerr << "SYNTAX ERROR 'ENDSIGNAL' but no 'SIGNAL'\n";
            delete newInstr;
            return 0;
        }
        if (fileContent[firstLine][0] . token == "SIGNAL") {
            newInstr -> instructionType = DO_SIGNAL;
            if (fileContent[firstLine] .size() != 1) {
                cerr << "SYNTAX ERROR Bad 'SIGNAL'\n";
                delete newInstr;
                return 0;
            }
            if (hanDler) // c'est qu'un traitant a déjà été écrit
            {
                cerr << "ERROR handler already specify\n";
                delete newInstr;
                return 0;
            }
            for(unsigned int k = firstLine + 1; k <= lastLine ; ++k) {
                if(fileContent[k][0] . token == "ENDSIGNAL") {
                    if(fileContent[k] . size() != 1) {
                        cerr << "SYNTAX ERROR Bad 'ENDSIGNAL' line " << k;
                        delete newInstr;
                        return 0;
                    }
                    unsigned int theNewLastLine   = k - 2;
                    int runningFirstLine = firstLine + 1;
                    while(theNewLastLine < k - 1) {
                        newInstr -> bodyInstr . push_back(
                            parseProg(fileContent,
                                      runningFirstLine,
                                      k - 1,qParsingVerbose, 
                                      &theNewLastLine));
                        if(0 == newInstr -> bodyInstr . back()) {
                            delete newInstr;
                            return 0;
                        }
                        runningFirstLine = theNewLastLine + 1;
                        newInstr -> bodyInstr . back() -> father = 
                            newInstr;
                    }
                    (*newLastLine) =  k;
                    if(qParsingVerbose) {
                        cerr << "\nParsing " << progName << ":" 
                             <<  (*newLastLine) << "...";
                        cerr << " -> END" << instructionKeyword[
                            newInstr -> instructionType];
                    }
                    hanDler = new ProcInstruction(*newInstr);
                    return newInstr;
                }
            } // fin boucle pour rechercher le ENDSIGNAL correspondant
              // qu'on n'a pas trouve, si l'on arrive ici
            cerr << "SYNTAX ERROR Missing 'ENDSIGNAL' do match 'SIGNAL'\n";
            delete newInstr;
            return 0;
        } // SIGNAL
        string instrKeyword(fileContent[firstLine][0] . token);
        if(fileContent[firstLine] . size() < 2                     ||
           fileContent[firstLine][1] . tokenType != INSTRTOK_OPER  ||
           fileContent[firstLine][1] . tokenOperType != OP_INSTR) {
            cerr << "SYNTAX ERROR Missing " << operChar[OP_INSTR] 
                 << " after keyword " << instrKeyword << "\n";
            delete newInstr;
            return 0;
        }
        if(instrKeyword == "WHILE") {
            newInstr -> instructionType = DO_WHILEREPEAT;
            const int enD = fileContent[firstLine] . size() - 2;
            if((fileContent[firstLine] . size() != 7   &&
                fileContent[firstLine] . size() != 9)                       || 
               fileContent[firstLine][2] . tokenType != INSTRTOK_NUMBER     ||
               fileContent[firstLine][3] . tokenType != INSTRTOK_OPER       ||
               fileContent[firstLine][3] . tokenOperType != OP_STARTWHILEARG||
               fileContent[firstLine][enD] . tokenType != INSTRTOK_OPER     ||
               fileContent[firstLine][enD] . tokenOperType != OP_ENDWHILEARG||
               fileContent[firstLine][enD+1] . token != "REPEAT"){ 
                cerr << "SYNTAX ERROR Bad 'WHILE'\n";
                delete newInstr;
                return 0;
            }
            if(fileContent[firstLine] . size() == 7) {
                newInstr -> operand . push_back(
                    findExistentSymbol(fileContent[firstLine][4] . token));
                if(newInstr -> operand . back() == -1) {
                    cerr << "ALG ERROR in WHILE Undefined symbol " 
                         << fileContent[firstLine][4] . token << "\n";
                    delete newInstr;
                    return 0;
                }            
                // ok, pas d'expression, alors un seul symbole. 
                // on cree l'expression "ce symbole != 0"
                newInstr -> operType = OP_DIFF;
                newInstr -> operand . push_back(
                    addNewSymbol(InstrToken("0",INSTRTOK_NUMBER,false)));
                
            }// pas d'expression
            else { // si, la, il y a une expression (9 tokens)
                newInstr -> operType = 
                    fileContent[firstLine][5] . tokenOperType;
                for(unsigned int kTkn = 4 ; kTkn < 7; kTkn += 2) {
                    switch(fileContent[firstLine][kTkn] . tokenType) {
                        case INSTRTOK_SYMBOL:
                            newInstr -> operand . push_back(
                                findExistentSymbol(
                                    fileContent[firstLine][kTkn] . token));
                            if(newInstr -> operand . back() == -1) {
                                cerr << "ALG ERROR in WHILE Undefined symbol " 
                                     << fileContent[firstLine][kTkn] . 
                                    token << "\n";
                                delete newInstr;
                                return 0;
                            }
                            break;
                        case INSTRTOK_NUMBER:
                        case INSTRTOK_STRING:
                            newInstr -> operand . push_back(
                                addNewSymbol(
                                    fileContent[firstLine][kTkn]));
                            break;
                        default: 
                            cerr << "SYNTAX ERROR Invalid token type " 
                                 << fileContent[firstLine][kTkn] . token 
                                 << "in WHILE\n";
                            delete newInstr;
                            return 0;
                    } // switch(tokenType)
                } // for(the two operands)
            }
            const int whileLabel(atoi(fileContent[firstLine][2] . 
                                      token . c_str()));
            for(unsigned int k = firstLine + 1; k <= lastLine ; ++k) {
                if(fileContent[k][0] . token == "ENDWHILE") {
                    if(fileContent[k] . size() != 3                    ||
                       fileContent[k][1] . tokenType != INSTRTOK_OPER  ||
                       fileContent[k][1] . tokenOperType != OP_INSTR   ||
                       fileContent[k][2] . tokenType != INSTRTOK_NUMBER) {
                        cerr << "SYNTAX ERROR Bad 'ENDWHILE' line " << k;
                        delete newInstr;
                        return 0;
                    }
                    const int endWhileLabel(atoi(fileContent[k][2] . 
                                             token . c_str()));
                    if(endWhileLabel == whileLabel) {
                        unsigned int theNewLastLine   = k - 2;
                        int runningFirstLine = firstLine + 1;
                        while(theNewLastLine < k - 1) {
                            newInstr -> bodyInstr . push_back(
                                parseProg(fileContent,
                                          runningFirstLine,
                                          k - 1,qParsingVerbose, 
                                          &theNewLastLine));
                            if(0 == newInstr -> bodyInstr . back()) {
                                delete newInstr;
                                return 0;
                            }
                            runningFirstLine = theNewLastLine + 1;
                            newInstr -> bodyInstr . back() -> father = 
                                newInstr;
                        }
                        (*newLastLine) =  k;
                        if(qParsingVerbose) {
                            cerr << "\nParsing " << progName << ":" 
                                 <<  (*newLastLine) << "...";
                            cerr << " -> END" << instructionKeyword[
                                newInstr -> instructionType];
                        }
                        return newInstr;
                    }
                }
            } // fin boucle pour rechercher le ENDWHILE correspondant
            // qu'on n'a pas trouve, si l'on arrive ici
            cerr << "SYNTAX ERROR Missing 'ENDWHILE " << whileLabel
                 << "'to match 'WHILE" << whileLabel << "'\n";
            delete newInstr;
            return 0;
        }// WHILE
        if(fileContent[firstLine][0] . token == "NEW") {
            newInstr -> instructionType = DO_NEW;
            *newLastLine = firstLine;
            if(fileContent[firstLine] . size() != 5                     ||
               fileContent[firstLine][2] . tokenType != INSTRTOK_SYMBOL || 
               fileContent[firstLine][3] . tokenType != INSTRTOK_OPER   ||
               fileContent[firstLine][3] . tokenOperType != OP_ASSIGN   ||
               (fileContent[firstLine][4] . tokenType != INSTRTOK_SYMBOL &&
                fileContent[firstLine][4] . tokenType != INSTRTOK_NUMBER)) {
                cerr << "SYNTAX ERROR : NEW <idVar> : <valOrIdVar>\n";
                delete newInstr;
                return 0;
            }
            if(findExistentSymbol(fileContent[firstLine][2] . token) != -1) {
                cerr << "ALG ERROR NEW leftvalue '" 
                     << fileContent[firstLine][2] . token
                     << "' already created.\n";
                delete newInstr;
                return 0;
            }
            newInstr -> leftValue = addNewSymbol(
                fileContent[firstLine][2]);
            switch(fileContent[firstLine][4] . tokenType) {
                case INSTRTOK_SYMBOL:
                    newInstr -> operand . push_back(
                        findExistentSymbol(
                            fileContent[firstLine][4] . token));
                    if(newInstr -> operand . back() == -1) {
                        cerr << "ALG ERROR in NEW Undefined symbol " 
                             << fileContent[firstLine][4] . token << "\n";
                        delete newInstr;
                        return 0;
                    }
                    if(qParsingVerbose) {
                        cerr << " -> " << instructionKeyword[
                            newInstr -> instructionType];
                    }
                    return newInstr;
                case INSTRTOK_NUMBER:
                    newInstr -> operand . push_back(
                        addNewSymbol(fileContent[firstLine][4]));
                    if(qParsingVerbose) {
                        cerr << " -> " << instructionKeyword[
                            newInstr -> instructionType];
                    }
                    return newInstr;
                default: 
                    cerr << "SYNTAX ERROR Invalid token " 
                         << fileContent[firstLine][4] . token 
                         << "in NEW\n";
                    delete newInstr;
                    return 0;
            } // switch(tokenType)
        } // if(NEW)
        if(fileContent[firstLine][0] . token == "COMPUTE" || 
           fileContent[firstLine][0] . token == "LOAD"    || 
           fileContent[firstLine][0] . token == "STORE"    ) {
            const string theKeyword(fileContent[firstLine][0] . token);
            ProcOperType theFirstOpT = OP_ASSIGN;
            if(fileContent[firstLine][0] . token == "STORE") {
                theFirstOpT = OP_MEMINDEX;
            }
            if(fileContent[firstLine] . size() != 7                       ||
               ((fileContent[firstLine][0] . token == "COMPUTE" ||
                 fileContent[firstLine][0] . token == "LOAD"      )   &&
                fileContent[firstLine][2] . tokenType != INSTRTOK_SYMBOL) ||
                (fileContent[firstLine][0] . token == "STORE"         && 
                 fileContent[firstLine][2] . tokenType == INSTRTOK_STRING)||
               fileContent[firstLine][3] . tokenType != INSTRTOK_OPER     ||
               fileContent[firstLine][3] . tokenOperType != theFirstOpT   ||
               fileContent[firstLine][5] . tokenType != INSTRTOK_OPER) {
                cerr << "SYNTAX ERROR Bad '" 
                     << theKeyword << "'\n";
                delete newInstr;
                return 0;
            }            
            newInstr -> instructionType = DO_COMP;
            *newLastLine = firstLine;
            if(theKeyword == "LOAD" ) {
                newInstr -> instructionType = DO_LOAD;
                if(fileContent[firstLine][5] . tokenOperType != OP_MEMINDEX) {
                    cerr << "SYNTAX ERROR Only " << operChar[OP_MEMINDEX]
                         << " allowed in 'LOAD's expression.\n";
                    delete newInstr;
                    return 0;
                }
                if((newInstr -> leftValue = findExistentSymbol(
                        fileContent[firstLine][2] . token)) == -1) {
                    cerr << "ALG ERROR in LOAD Undefined symbol " 
                         << fileContent[firstLine][2] . token << "\n";
                    delete newInstr;
                    return 0;
                }
            }
            else if(theKeyword == "STORE") {
                newInstr -> instructionType = DO_STORE;
                if(fileContent[firstLine][5] . tokenOperType != OP_ASSIGN) {
                    cerr << "SYNTAX ERROR Only " << operChar[OP_ASSIGN]
                         << " allowed in  'STORE's expression after "
                         << operChar[OP_MEMINDEX] << "<index>\n";
                    delete newInstr;
                    return 0;
                }
                if(fileContent[firstLine][2] . tokenType == INSTRTOK_SPECIAL) {
                    if(fileContent[firstLine][2] . token == "_") {
                        newInstr -> leftValue = THE_SHARED_MEMORY;
                    }
                    else {
                        cerr << "SYNTAX ERROR Only _ (for shared memory) "
                             << " allowed as 'STORE's leftvalue base\n";
                        delete newInstr;
                        return 0;
                    }
                }
                else {
                    if((newInstr -> leftValue = findExistentSymbol(
                            fileContent[firstLine][2] . token)) == -1) {
                        cerr << "ALG ERROR in STORE Undefined symbol " 
                             << fileContent[firstLine][2] . token << "\n";
                        delete newInstr;
                        return 0;
                    }
                }
            }
            else { // donc DO_COMP
                if((newInstr -> leftValue = findExistentSymbol(
                        fileContent[firstLine][2] . token)) == -1) {
                    cerr << "ALG ERROR in COMPUTE Undefined symbol " 
                         << fileContent[firstLine][2] . token << "\n";
                    delete newInstr;
                    return 0;
                }
            }
            newInstr -> operType = fileContent[firstLine][5] . tokenOperType;
            for(unsigned int kTkn = 4 ; kTkn < 7; kTkn += 2) {
                switch(fileContent[firstLine][kTkn] . tokenType) {
                case INSTRTOK_SYMBOL:
                    newInstr -> operand . push_back(
                        findExistentSymbol(
                            fileContent[firstLine][kTkn] . token));
                    if(newInstr -> operand . back() == -1) {
                        cerr << "ALG ERROR in "
                             << theKeyword << " Undefined symbol " 
                             << fileContent[firstLine][kTkn] . token << "\n";
                        delete newInstr;
                        return 0;
                    }
                    break;
                case INSTRTOK_NUMBER:
                case INSTRTOK_STRING:
                    newInstr -> operand . push_back(
                        addNewSymbol(
                            fileContent[firstLine][kTkn]));
                    break;
                    case INSTRTOK_SPECIAL:
                        if(theKeyword != "LOAD" ||
                            kTkn != 4) {
                            if(newInstr -> operand . back() == -1) {
                                cerr << "ALG ERROR in " << theKeyword
                                     << ", "
                                     << "forbiden special "
                                     << fileContent[firstLine][kTkn] . token
                                     << " in expression.\n";
                                delete newInstr;
                                return 0;
                            }
                        }
                    newInstr -> operand . push_back(THE_SHARED_MEMORY);
                    break;
                default: 
                    cerr << "SYNTAX ERROR Invalid token " 
                         << fileContent[firstLine][kTkn] . token 
                         << " in "
                         << theKeyword << "\n";
                    delete newInstr;
                    return 0;
                } // switch(tokenType)
            } // for(the two operands)
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[
                    newInstr -> instructionType];
            }
            return newInstr;
        }// if(COMPUTE or LOAD or STORE)
        if(fileContent[firstLine][0] . token == "READ") {
            if(fileContent[firstLine] . size() != 3                     ||
               fileContent[firstLine][2] . tokenType != INSTRTOK_SYMBOL) {
                cerr << "SYNTAX ERROR Bad 'READ'...\n";
                delete newInstr;
                return 0;
            }
            newInstr -> instructionType = DO_READ;
            *newLastLine = firstLine;
            if((newInstr -> leftValue = findExistentSymbol(
                fileContent[firstLine][2] . token)) == -1) {
                cerr << "ALG ERROR in READ Undefined symbol " 
                     << fileContent[firstLine][2] . token << "\n";
                delete newInstr;
                return 0;
            }
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[
                    newInstr -> instructionType];    
            }
            return newInstr;
        }// if(READ)
        if(fileContent[firstLine][0] . token == "COPY") {
            if(fileContent[firstLine] . size() != 5                     ||
               fileContent[firstLine][2] . tokenType != INSTRTOK_SYMBOL ||
               fileContent[firstLine][3] . tokenType != INSTRTOK_OPER   ||
               fileContent[firstLine][3] . tokenOperType != OP_ASSIGN) {
                cerr << "SYNTAX ERROR Bad 'COPY'...\n";
                delete newInstr;
                return 0;
            }
            newInstr -> instructionType = DO_COPY;
            *newLastLine = firstLine;
            if((newInstr -> leftValue = findExistentSymbol(
                fileContent[firstLine][2] . token)) == -1) {
                cerr << "ALG ERROR in COPY Undefined symbol " 
                             << fileContent[firstLine][2] . token << "\n";
                delete newInstr;
                return 0;
            }
            switch(fileContent[firstLine][4] . tokenType) {
                case INSTRTOK_SYMBOL:
                    newInstr -> operand . push_back(
                        findExistentSymbol(
                            fileContent[firstLine][4] . token));
                    if(newInstr -> operand . back() == -1) {
                        cerr << "ALG ERROR in COPY Undefined symbol " 
                             << fileContent[firstLine][4] . token << "\n";
                        delete newInstr;
                        return 0;
                    }
                    break;
                case INSTRTOK_NUMBER:
                    newInstr -> operand . push_back(
                        addNewSymbol(
                            fileContent[firstLine][4]));
                    break;
                case INSTRTOK_STRING:
                    cerr << "SYNTAX ERROR Invalid string " 
                         << fileContent[firstLine][4] . token 
                         << "in COPY. Only integers or variables.\n";
                    delete newInstr;
                    return 0;
                default: 
                    cerr << "SYNTAX ERROR Invalid token " 
                         << fileContent[firstLine][4] . token 
                         << "in COPY\n";
                    delete newInstr;
                    return 0;
                } // switch(tokenType)
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[
                    newInstr -> instructionType];
            }
            return newInstr;
        }// if(COPY)
        if(fileContent[firstLine][0] . token == "PRINT") {
            newInstr -> instructionType = DO_PRINT;
            *newLastLine = firstLine;
            for(unsigned int kTkn = 2 ; 
                kTkn < fileContent[firstLine] . size(); 
                kTkn++) {
                if(kTkn % 2) {
                    if(fileContent[firstLine][kTkn] . tokenType !=
                       INSTRTOK_OPER ||
                       fileContent[firstLine][kTkn] . tokenOperType !=
                       OP_ENUM) {
                        cerr << "SYNTAX ERROR in PRINT Missing enum oper "
                             << operChar[OP_ENUM] << "\n";
                        delete newInstr;
                        return 0;
                    }
                    continue;
                }
                switch(fileContent[firstLine][kTkn] . tokenType) {
                case INSTRTOK_SYMBOL:
                    newInstr -> operand . push_back(
                        findExistentSymbol(
                            fileContent[firstLine][kTkn] . token));
                    if(newInstr -> operand . back() == -1) {
                        cerr << "ALG ERROR in PRINT Undefined symbol " 
                             << fileContent[firstLine][kTkn] . token << "\n";
                        delete newInstr;
                        return 0;
                    }
                    break;
                case INSTRTOK_NUMBER:
                case INSTRTOK_STRING:
                    newInstr -> operand . push_back(
                            addNewSymbol(
                            fileContent[firstLine][kTkn]));
                    break;
                default: 
                    cerr << "SYNTAX ERROR Invalid token " 
                         << fileContent[firstLine][kTkn] . token 
                         << "in PRINT\n";
                    delete newInstr;
                    return 0;
                } // switch(tokenType)
            } // for(each token)
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[
                    newInstr -> instructionType];
            }
            return newInstr;
        }// if(PRINT)
        if(fileContent[firstLine][0] . token == "FORK") {
            if(fileContent[firstLine] . size() < 3) {
                cerr << "SYNTAX ERROR FORK missing leftvalue\n";
                delete newInstr;
                return 0;
            }
            newInstr -> instructionType = DO_FORK;
            *newLastLine = firstLine;
            switch(fileContent[firstLine][2] . tokenType) {
                case INSTRTOK_SYMBOL:
                    newInstr -> leftValue = 
                        findExistentSymbol(
                            fileContent[firstLine][2] . token);
                    if(newInstr -> leftValue == -1) {
                        cerr << "ALG ERROR in FORK Undefined symbol " 
                             << fileContent[firstLine][2] . token << "\n";
                        delete newInstr;
                        return 0;
                    }
                    return newInstr;
                default: 
                    cerr << "SYNTAX ERROR Invalid token " 
                         << fileContent[firstLine][2] . token 
                         << "in FORK\n";
                    delete newInstr;
                    return 0;
            } // switch(tokenType)
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[
                    newInstr -> instructionType];
            }
            return newInstr;
        }// if(FORK)
        if(fileContent[firstLine][0] . token == "MUTEX") {
            if(fileContent[firstLine] . size() != 5 ||
               fileContent[firstLine][2] . tokenType != INSTRTOK_SPECIAL ||
               fileContent[firstLine][2] . token     != "_"              ||
               fileContent[firstLine][3] . tokenType != INSTRTOK_OPER    ||
               fileContent[firstLine][3] . tokenOperType != OP_ASSIGN    ||
               fileContent[firstLine][4] . tokenType != INSTRTOK_SPECIAL ||
               (fileContent[firstLine][4] . token != "_P" &&
                fileContent[firstLine][4] . token != "_V")) {
                cerr << "SYNTAX ERROR Bad 'MUTEX'...\n";
                delete newInstr;
                return 0;
            }
            newInstr -> instructionType = DO_MUTEX;
            *newLastLine = firstLine;
            switch(fileContent[firstLine][4] . token[1]) {
                case 'P': newInstr -> operand . push_back(MUTEX_OPER_P);break;
                case 'V': newInstr -> operand . push_back(MUTEX_OPER_V);break;
                default: cerr << "INTERNAL ERROR Unexpected "
                              << " mutex operation " 
                              << fileContent[firstLine][4] . token << "\n";
                    delete newInstr;
                    return 0;
            }
            newInstr -> leftValue = THE_MUTEX;
            if(qParsingVerbose) {
                cerr << " -> " << instructionKeyword[
                                    newInstr -> instructionType];
            }
            return newInstr;
        }// if(MUTEX)
        if(fileContent[firstLine][0] . token == "SIGADD" ||
           fileContent[firstLine][0] . token == "SIGDEL") {
            if (fileContent[firstLine] . size() != 3 ||
                fileContent[firstLine][2] . tokenType != INSTRTOK_NUMBER ){
                cerr << "SYNTAX ERROR expected SIGADD @ <numsig>\n";
                delete newInstr;
                return 0;
            }

            {
                istringstream istr (fileContent[firstLine][2] . token);
                istr >> newInstr -> leftValue;
                if (istr.fail()) { // normalement on est bon car
                                   // on a testé si c'étais un numéro
                                   // mais on sait jamais
                    cerr << "oops\n";
                    delete newInstr;
                    return 0;
                }
            }
            if (newInstr -> leftValue == SIGKILL ||
                newInstr -> leftValue == SIGSTOP ||
                newInstr -> leftValue == SIGCONT ||
                newInstr -> leftValue == SIGQUIT ||
                0  >= newInstr -> leftValue      ||
                32 <  newInstr -> leftValue) {
                cerr << "ERROR invalid signal\n";
                delete newInstr;
                return 0;
            }

            if (fileContent[firstLine][0] . token == "SIGADD")
                newInstr -> instructionType = DO_SIGADD;
            else // forcément SIGDEL
                newInstr -> instructionType = DO_SIGDEL;

            *newLastLine = firstLine;

            return newInstr;

        }// if(SIGADD)

        cerr << "SYNTAX ERROR Unknown instruction keyword "
             << fileContent[firstLine][0] . token << "\n";
        delete newInstr;
        return 0;
    } // parseProg()

    // constructeur qui lit et parse les fichiers

    ProcInfo::ProcInfo(const string &fileList, bool qMnSV, 
                       bool qTokV, bool qPrsV, bool qExecV) :
        qMainStepsVerbose(qMnSV),
        qTokenizingVerbose(qTokV),
        qParsingVerbose(qPrsV),        
        qExecutingVerbose(qExecV)        
    {
        // on ouvre et lit ligne par ligne les fichiers passes en arguments, 
        // un par un
        // la methode ProcData::parseProg() s'occupe du parsing effectif 
        // de chaque ligne
        // elle rend zero si et seulement si tout va bien, i.e. pas 
        // d'erreur de syntaxe 
        sharedMemoryBase  = 0;
        sharedMemoryLimit = memoryLimitForAll;
        sharedMemory . resize(sharedMemoryLimit);
        mutex             = 1;
        istringstream buffStr(fileList);
        for(string fileName; buffStr >> fileName;) { // pour chaque fichier
            ifstream progFile (fileName . c_str());
            if(!progFile) {
                cerr << "ERROR Bad file name " << fileName << ", skipping.\n";
                continue;
            }
            procData . push_back(new ProcData(fileName,memoryLimitForAll));
            if(qMainStepsVerbose) {
                cerr << "INFO [" <<   procData . size() - 1 << "]"
                     << " Reading " << fileName << " line by line ...\n" ;
            }
            bool qOk (true);
            ProgToken fileContent; 
            for(string fileLine; getline(progFile, fileLine);) {
                stripWhiteSpace(fileLine);
                deque<InstrToken> fileLineContent;
                if(tokenizeInstr(fileLine,fileLineContent)) {
                    // oops, erreur(s) de syntaxe
                    delete procData . back();
                    procData . pop_back(); // et alors on y renonce  
                    qOk = false;
                    break;
                }
                fileContent . push_back(fileLineContent);
                if(qTokenizingVerbose) {
                    cerr << "Tokens for line "  << fileContent . size() << " ";
                    for(unsigned int k = 0; k < fileLineContent . size(); 
                    ++k) {
                    cerr << "'" << fileLineContent[k] . token << "' ";
                    }
                    cerr << " ::\n";
                }
            }
            progFile . close();
            if(fileContent . size() == 0) {
                cerr << "Empty program file " << fileName << ", skipping.\n";
                delete procData . back();
                procData . pop_back(); // et alors on y renonce  
                qOk = false; 
            }
            if(0 == (procData . back() -> proGram =
                     procData . back() -> parseProg(fileContent, 0,
                                                    fileContent . size() - 1, 
                                                    qParsingVerbose, 0))) {
                // oops, erreur(s) de syntaxe
                delete procData . back();
                procData . pop_back(); // et alors on y renonce  
                qOk = false; 
            }
            else {
                if(procData . back() -> proGram -> instructionType != 
                   DO_PROGRAM) {
                    cerr << "SYNTAX ERROR: expected \n"
                         << "'PROGRAM'\n...\n'ENDPROGRAM'\n";
                    delete procData . back();
                    procData . pop_back(); // et alors on y renonce  
                    qOk = false; 
                }
            }
            if(qOk) { // tout va bien pour ce fichier, 
                // quelques initialisations restent a faire :
                // Copie de proGram pour pouvoir redémarrer le processus
                procData.back() -> proGramInit = new ProcInstruction(
                                                *procData.back() -> proGram);
                // vidage puis ajout de SIGQUIT au masque
                Sigemptyset (&procData . back() -> sigMask);
                Sigaddset   (&procData . back() -> sigMask, SIGQUIT);
                if(qParsingVerbose) {
                    cerr << "ok.\n";
                }
            }
        } // fin de la boucle pour chaque fichier 
        outstandingProcCount = procData . size();
        qDoSleepAfterEachInstruction = false;
        ::srand(::getpid());
    } // fin de ProcInfo::ProcInfo()
    
    
    // *************************************************** //
    // Ce qui suit sert pour l'execution des processus   * //
    // *************************************************** //
   
    // rappel : il y a des instructions simples (NEW, COMPUTE, etc.)
    // et des instructions complexe (WHILE .. REPEAT ... ENDWHILE
    // ainsi que le programme lui-meme) ; 
    // une instruction complexe comporte un corps -- bodyInstr --
    // contenant des pointeurs vers les instructions composantes
    // (qui peuvent etre simples ou complexes)

    // CHAQUE instruction complexe possede son propre compteur ordinal

    // on avance dans le programme en descendant recursivement 
    // jusqu'a la premiere instruction simple ou condition de WHILE
      
    // le modele d'execution choisi ici execute donc une instruction 
    // SIMPLE a la fois ; pour les instructions complexes,
    // lorsqu'on vient de les commencer, on evalue leur condition, 
    // et on initialise leur sous-compteur ordinal (et on a fini ce pas)
    // par la suite, a l'iteration d'apres, on avance dans les
    // instructions du corps, une par une, de maniere recursive 

    // l'execution se fait a travers l'appel a ProcInfo::updateProcData()
    // avec l'ordre 'ADVANCE_PROC' (voir en haut, tout au debut), qui 
    // appelle ainsi doOneStepAndAdvancePC() (voir plus bas), laquelle 
    // appelle doTheInstruction() ou doTheExpressionOfThe(). 

    //*****************************************************************
    // 
    // Des fonctions auxiliaires
    
    // fonction pour terminer un processus 

    void ProcInfo::doTerminateProc(const int procPid) {
    if (STATUS == STAT_TRACEEND     ||
        STATUS == STAT_TRACESTEPRUN)
        cout << "Processus terminé : "
             << procData[procPid] -> progName
             << '\n';
        procData[procPid] -> procStatus = STAT_TERMINATED;
        --outstandingProcCount;
    }


    //*****************************************************************
    // 
    // Les fonctions qui font effectivement le travail 
    
    bool ProcInfo::doTheInstruction(const int procPid, 
                                    ProcInstruction *crtInstr,
                                    int *forkedPid   /* = 0*/,
                                    ProcInstruction **forkedInstr /* = 0 */) {
        // renvoie vrai s'il y a eu une erreur, faux sinon
        bool returnValue(false);
        if(forkedInstr) *forkedInstr = 0;
        if(forkedPid)   *forkedPid   = 0;
        if(crtInstr -> instructionType != DO_PRINT  &&
           crtInstr -> instructionType != DO_STORE  &&
           crtInstr -> instructionType != DO_SIGNAL &&
           crtInstr -> instructionType != DO_MUTEX) {
            if(crtInstr -> leftValue < 0 ||
               crtInstr -> leftValue > 
               (int)procData[procPid] -> symbolTable . size() - 1) {
                cerr << "INTERNAL ERROR, NEW leftValue symbol index"
                     << " got corrupted " << crtInstr -> leftValue  << " "
                     << crtInstr -> fileName << ":" 
                     << crtInstr -> lineNumber+1 << "\n";
                return true;
            }
        }
        switch(crtInstr -> instructionType) {
            case DO_NEW: {
                procData[procPid] -> 
                        symbolTable[
                            crtInstr -> leftValue] . value = 
                    procData[procPid] -> symbolTable[
                        crtInstr -> operand[0]] . value;
                break;
            }// DO_NEW
            case DO_COMP: {
                bool qError (false);
                int result(doTheExpressionOfThe(procPid,crtInstr,&qError));
                if(!qError) {
                    procData[procPid] -> 
                        symbolTable[
                            crtInstr -> leftValue] . value = 
                        result;
                }
                returnValue = qError;
                break;
            }// DO_COMP
            case DO_COPY:
                procData[procPid] -> 
                        symbolTable[
                            crtInstr -> leftValue] . value = 
                    procData[procPid] -> 
                        symbolTable[
                            crtInstr -> operand[0]] . value;
                    break;
                // DO_COPY
            case DO_READ: {
                ProcStatus oldStat (procData[procPid] -> procStatus);
                procData[procPid] -> procStatus = STAT_IOWAIT; 

                // "Sécurisation" de l'entrée au clavier, par contre
                // maintenant, on ne peux plus entrer juste "4 5" par exemple,
                // il faut taper <Entrer> entre les deux entiers
                while (true) {
                    string Str;
                    getline(cin, Str);
                    if (Str.empty()) {
                        cin.clear();
                        cerr << "INPUT ERROR expected int, try again\n";
                        continue;
                    }

                    istringstream sstr (Str);
                    sstr >> (procData[procPid] ->
                             symbolTable[
                                 crtInstr -> leftValue] . value);
                    if (sstr.fail()) {
                        cerr << "INPUT ERROR expected int, try again\n";
                        continue;
                    }
                    break;
                }

                procData[procPid] -> procStatus = oldStat;
                break;
            }//DO_READ
            case DO_PRINT: {
                ProcStatus oldStat (procData[procPid] -> procStatus);
                procData[procPid] -> procStatus = STAT_IOWAIT; 
                for(unsigned int k = 0;k < crtInstr -> operand . size(); ++k){
                    if(procData[procPid] -> 
                       symbolTable[crtInstr -> operand[k]] . opType
                       == OPND_TYPE_INT) {
                        cout << procData[procPid] ->
                            symbolTable[crtInstr -> operand[k]] . value ;
                    }
                    else {
                        cout << procData[procPid] ->
                            symbolTable[crtInstr -> operand[k]] . strValue;
                    }
                }
                cout << flush;
                procData[procPid] -> procStatus = oldStat;
                break;
            } // DO_PRINT
            case DO_STORE:{
                const int memBase (crtInstr -> leftValue == 
                                   THE_SHARED_MEMORY?
                                   sharedMemoryBase: // ??/proc...
                                   procData[procPid] ->
                                   symbolTable[crtInstr -> leftValue] . value);
                const int memIndex (procData[procPid] ->
                        symbolTable[crtInstr -> operand[0]] . value);
                const int theMemLimit (crtInstr -> leftValue == 
                                       THE_SHARED_MEMORY?
                                       sharedMemoryLimit : 
                                       procData[procPid] -> heapMemoryLimit);
                if(memBase > theMemLimit) {
                    cerr << "RUN ERROR memBase out of bounds in STORE, "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return true;
                }
                if(memBase+memIndex > theMemLimit) {
                    cerr << "RUN ERROR memBase+memIndex out of bounds "
                         << " in STORE, "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return true;
                }
                vector<int>::iterator destination(crtInstr -> leftValue == 
                    THE_SHARED_MEMORY?
                    sharedMemory . begin():
                    procData[procPid] -> heapMemory . begin());
                destination += memBase+memIndex;
                (*destination) =  procData[procPid] ->
                    symbolTable[crtInstr -> operand[1]] . value;
                break;
            }// DO_STORE
            case DO_LOAD: {
                const int memBase (crtInstr -> operand[0] 
                                   == THE_SHARED_MEMORY?
                                   sharedMemoryBase: // ??/proc...
                                   procData[procPid] ->
                                   symbolTable[
                                       crtInstr -> operand[0]] . value);
                const int memIndex (procData[procPid] ->
                        symbolTable[crtInstr -> operand[1]] . value);
                const int theMemLimit (crtInstr -> operand[0] == 
                                       THE_SHARED_MEMORY?
                                       sharedMemoryLimit : 
                                       procData[procPid] -> heapMemoryLimit);
                if(memBase > theMemLimit) {
                    cerr << "RUN ERROR memBase out of bounds in LOAD, "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return true;
                }
                if(memBase+memIndex > theMemLimit) {
                    cerr << "RUN ERROR memBase+memIndex out of bounds "
                         << " in LOAD, "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return true;
                }
                vector<int>::iterator source(crtInstr -> operand[0] ==  
                    THE_SHARED_MEMORY?
                    sharedMemory . begin():
                    procData[procPid] -> heapMemory . begin());
                source += memBase+memIndex;
                procData[procPid] ->
                    symbolTable[crtInstr -> leftValue] . value = 
                    (*source);
                break;
            }// DO_LOAD
            case DO_FORK:
                procData[procPid] -> symbolTable[
                    crtInstr -> leftValue] . value = 0;
                    procData . resize(procData . size() + 1);
                procData[procData . size() - 1] = new ProcData(
                    *(procData[procPid]));
                procData[procPid] -> symbolTable[
                    crtInstr -> leftValue] . value = procData . size() - 1;
                // le nouveau fils vient d'etre cree par dedoublement
                procData . back() -> procStatus = STAT_WAITING; 
                scheduler -> enQueueProc(procData . size() - 1);
                ++outstandingProcCount;
                // et maintenant on prend soin du pere aussi
                procData[procPid] ->  procStatus = STAT_SYS;
                if(forkedInstr) {
                    (*forkedInstr) = 
                        findCrtInstruction(procData . back() -> proGram);
                }
                if(forkedPid) {
                    *forkedPid = procData . size() - 1;
                }
                break;
                // DO_FORK
            case DO_MUTEX: {
                const int mutexOper (crtInstr -> operand[0]);
                if(crtInstr -> leftValue != THE_MUTEX) {
                    cerr << "INTERNAL ERROR Unexpected leftvalue "
                         << crtInstr -> leftValue 
                         << "in DO_MUTEX, "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return true;
                }
                switch(mutexOper){
                    case MUTEX_OPER_P: if(mutex == 1) {
                        mutex = 0;
                        procData[procPid] -> procMutexStatus = STAT_MUTEXGRAB;
                    }
                    else {
                        procData[procPid] -> procMutexStatus = STAT_MUTEXWAIT;
                    }
                        break;
                    case MUTEX_OPER_V: if(mutex == 0) {
                        mutex = 1; 
                        procData[procPid] -> procMutexStatus = STAT_NOMUTEX;
                        break;
                    default: cerr << "INTERNAL ERROR Unexpected mutex"
                                  << " operation value " 
                                  << mutexOper << ", "
                                  << crtInstr -> fileName << ":" 
                                  << crtInstr -> lineNumber+1 << "\n";
                        return true;
                    }
                } // switch(mutexOper)
                break;
            }// DO_MUTEX
            case DO_SIGADD: {
                Sigaddset (&procData[procPid] -> sigMask,
                                           crtInstr -> leftValue);
                break;
            }// DO_SIGADD
            case DO_SIGDEL: {
                Sigdelset (&procData[procPid] -> sigMask,
                                            crtInstr -> leftValue);
                break;
            }// DO_SIGDEL
            default: ;
        } // switch(type de l'instruction)
        if(qDoSleepAfterEachInstruction) {
            sleep(ONE_INSTRUCTION_SLEEP);// on se "repose" un peu 
        }
        return returnValue;
    } // doTheInstruction()


    int ProcInfo::doTheExpressionOfThe(const int       procPid, 
                                       ProcInstruction *crtInstr,
                                       bool            *pQError) {
        int result(0);
        switch(crtInstr -> operType) {
            case OP_ADD:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value
                        + procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_SUB:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        - procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_MUL:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        * procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_DIV:
                if(procData[procPid] -> symbolTable[
                       crtInstr -> operand[1]] . value) {
                    return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value
                            / procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
                }
                else {
                    (*pQError) = true;
                    cerr << "RUN ERROR Division by zero in '/', "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return 0;
                }
            case OP_REM:
                if(procData[procPid] -> symbolTable[
                       crtInstr -> operand[1]] . value) {
                    return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                            % procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
                }
                else {
                    (*pQError) = true;
                    cerr << "RUN ERROR Division by zero in '%', "
                         << crtInstr -> fileName << ":" 
                         << crtInstr -> lineNumber+1 << "\n";
                    return 0;
                }
            case OP_GT:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        > procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_LT:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        < procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_GE:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        >= procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_LE:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        <= procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_EQ:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        == procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_DIFF:
                return (procData[procPid] -> symbolTable[
                            crtInstr -> operand[0]] . value 
                        != procData[procPid] -> symbolTable[
                            crtInstr -> operand[1]] . value);
            case OP_NOP:
            case OP_INSTR:
            case OP_ENUM:
            case OP_ASSIGN:
            case OP_STARTWHILEARG:
            case OP_ENDWHILEARG:
            case OP_STARTSTRING:
            case OP_ENDSTRING:
            case OP_MEMINDEX:
                cerr << "RUN ERROR Forbidden operator "
                     << operChar[crtInstr -> operType];
                cerr << " in expression computation, " 
                     << crtInstr -> fileName << ":" 
                     << crtInstr -> lineNumber+1 << "\n";
                return 0;
            default: cerr << "INTERNAL ERROR Would have expected an "
                          << " operator code in " 
                          << crtInstr -> operType << ", "
                          << crtInstr -> fileName << ":" 
                          << crtInstr -> lineNumber+1 << "\n";
            return 0;
        }
        return(result);        
    } // doTheExpressionOfThe()

    // La fonction qui suit maintenant, nommee doOneStepAndAdvancePC() 
    // est le "coeur" du mecanisme : elle execute l'instruction courante, 
    // et puis avance le ou les programCounter d'un cran;
    // son role est d'accommoder les instructions complexes et la
    // structure arborescente (donc les imbrications) 
    //
    // l'execution effective d'une instruction simple est faite par 
    // doTheInstruction() (voir plus haut), et l'evaluation de la 
    // condition d'un WHILE par doTheExpressionOfThe()
    // 
    // pour bien explorer la structure arborescente du programme,
    // doOneStepAndAdvancePC() va le plus en profondeur possible 
    //   while(...) { <= si condition vraie, on descend
    //     ... 
    //     if(......)  { <= apres, si condition vraie, on descend encore
    //       ...
    //       while(...) { <= etc.
    //         ....         <== enfin ici, et puis on revient dans 
    //                          les if/while englobants
    //    }}}
    // 

    // invariant: programCounter est toujours entre -1 et bodyInstr . size()-1
    // (y compris ces deux bornes)

    ProcInfo::ProcAdvanceType 
    ProcInfo::doOneStepAndAdvancePC(const int         procPid,
                                    ProcInfo::ProcInstruction * crtInstr,
                                    int             * forkedPid /* = 0*/,
                                    ProcInfo::ProcInstruction 
                                    **forkedInstr /* = 0*/) {
        if(crtInstr -> instructionType != DO_WHILEREPEAT && 
           crtInstr -> instructionType != DO_PROGRAM) {
            if(doTheInstruction(procPid,crtInstr,forkedPid,forkedInstr)) {
                // c'est qu'il y a eu une erreur grave
                doTerminateProc(procPid);
                return ADV_ONE_MORE_STEP_INSIDE;
            }
            if(procData[procPid] -> procMutexStatus == STAT_MUTEXWAIT) {
                // alors on n'avance pas le compteur ordinal, et on patiente
                return ADV_ONE_MORE_STEP_INSIDE; // facon de parler
                // en fait c'est pour dire "il n'y a rien a faire"
            }
            if(crtInstr->instructionType == DO_FORK && *forkedInstr) {
                // travail effectif, et il s'agissait d'un FORK
                (*forkedInstr) = (*forkedInstr) -> father;
                // pour la propagation, pour l'incrementation
               return ADV_REACHED_END_AND_NOW_DUPLICATED;
           }
           // sinon, travail effectif mais "normal"
           return ADV_REACHED_END;
           // pour que la fonction qui nous appelle tente d'incrementer le
           // programCounter a son niveau, ici on n'a rien a incrementer
        }
        // sinon, c'est que nous sommes DANS un WHILE, 
        // ou bien au plus haut niveau du programme
        switch(crtInstr -> condEval) {
            case COND_NOT_EVAL: // bon, on doit voir
                 if(crtInstr -> instructionType != DO_PROGRAM) {

                    bool qError (false);
                    crtInstr -> condEval = 
                        (doTheExpressionOfThe(procPid,crtInstr,&qError)?
                         COND_EVAL_TRUE:COND_EVAL_FALSE);
                    if(qError) {
                        doTerminateProc(procPid);
                    }
                    if(crtInstr -> condEval == COND_EVAL_TRUE) {
                        if(crtInstr -> bodyInstr . size()) {
                            crtInstr -> programCounter = 0; // pour le coup
                            // d'apres, car la prochaine instruction a
                            // executer est la premiere du corps du WHILE
                            procData[procPid] -> nextLineNumber = 
                                crtInstr -> bodyInstr[
                                    crtInstr -> programCounter] -> lineNumber;
                        }
                        else { // WHILE de corps vide
                            crtInstr -> programCounter = -1; // invariant
                            crtInstr -> condEval       = COND_NOT_EVAL;
                        }
                    }
                    // travail effectif effectue, on est bon
                    return ADV_ONE_MORE_STEP_INSIDE;
                    // et il n'y a plus rien a faire au retour
                }
                // sinon on passe simplement au cas suivant
            case COND_EVAL_TRUE:{
                // c'est qu'on a deja evalue la condition lors d'un
                // appel precedent, donc maintenant il faut voir ou
                // nous en sommes localement, dans ce corps
                if(crtInstr -> programCounter == -1) { 
                    // ah, on est en haut du corps
                    crtInstr -> programCounter = 0; 
                    // et on va alors faire la premiere instruction
                }
                  // on doit effectuer un appel recursif qui 
                // "s'en occupe" effectivement
                ProcInstruction * newlyForkedInstruction;
                int               newlyForkedPid;
                ProcAdvanceType result(doOneStepAndAdvancePC(
                                       procPid,
                                       crtInstr -> 
                                       bodyInstr[crtInstr -> 
                                             programCounter],
                                       &newlyForkedPid,
                                       &newlyForkedInstruction));
                switch(result) {
                    case ADV_ONE_MORE_STEP_INSIDE: // c'est tout bon,
                        // les choses ont avance a l'interieur, donc ici
                        // on ne bouge pas pour le moment, i.e. on n'a
                        // rien a faire pour l'instant a ce niveau-ci
                        return ADV_ONE_MORE_STEP_INSIDE;
                        // et on le dit pour plus haut egalement
                    case ADV_REACHED_END: // ah, on doit faire quelque chose
                    case ADV_REACHED_END_AND_NOW_DUPLICATED: // pareil, et +
                        if(crtInstr -> programCounter < 
                           (int)crtInstr -> bodyInstr . size() - 1) {
                            // aha, on aura encore quoi faire a ce niveau
                            crtInstr -> programCounter++;
                            procData[procPid] -> nextLineNumber = 
                                crtInstr -> bodyInstr[
                                    crtInstr -> programCounter] -> lineNumber;
                            // et donc on avance dans ce corps
                            if(result == ADV_REACHED_END_AND_NOW_DUPLICATED) {
                                newlyForkedInstruction -> programCounter++;
                                // on avance dans le nouveau corps egalement
                                procData[newlyForkedPid] -> nextLineNumber = 
                                newlyForkedInstruction -> bodyInstr[
                                    newlyForkedInstruction -> programCounter] 
                                -> lineNumber;

                            }
                            return ADV_ONE_MORE_STEP_INSIDE;
                            // et on "rassure" la fonction qui nous a appele
                            // car il n'y a plus rien a faire plus haut
                        }
                        else { // eh non, on a fini a ce niveau egalement
                            crtInstr -> programCounter = -1; // invariant
                            crtInstr -> condEval       = COND_NOT_EVAL;
                            procData[procPid] -> nextLineNumber =  
                                crtInstr -> lineNumber;
                            if(crtInstr -> instructionType == DO_PROGRAM) {
                                doTerminateProc(procPid);
                                // car on a tout fini
                            }
                            // pour preparer une eventuelle reexecution
                            if(result == ADV_REACHED_END_AND_NOW_DUPLICATED) {
                                newlyForkedInstruction -> 
                                    programCounter = -1;
                                newlyForkedInstruction -> 
                                    condEval       = COND_NOT_EVAL;
                                if(newlyForkedInstruction -> instructionType
                                   == DO_PROGRAM) {
                                    procData[newlyForkedPid] 
                                    -> nextLineNumber = 1;
                                    doTerminateProc(newlyForkedPid);
                                    return ADV_REACHED_END_AND_NOW_DUPLICATED;
                                    // de toutes manieres ce sera ignore
                                    // car on est au plus haut niveau
                                }
                                // sinon c'est un WHILE,donc on devra reevaluer
                                // sa condition avant de dire qu'on a fini...
                                // et ce sera lors du prochain appel
                                return ADV_ONE_MORE_STEP_INSIDE;
                                // on "rassure" la fonction qui nous a appele
                                // car il n'y a plus rien a faire plus haut
                            } // fin if(on a eu un FORK aussi)
                            // sinon, pas de FORK, et c'est tout "simple"
                            if(crtInstr -> instructionType == DO_PROGRAM) { 
                                // fini pour de bon
                                procData[procPid] -> nextLineNumber = 1;
                                return ADV_REACHED_END;
                                // on avertit la fonction qui nous a appele,
                                // pour qu'elle avance le PC de son niveau
                                // (pour DO_PROGRAM ce sera ignore)
                                }
                                // sinon c'est un WHILE, donc on devra reevaluer
                                // sa condition avant de dire qu'on a fini...
                                // et ce sera lors du prochain appel
                                return ADV_ONE_MORE_STEP_INSIDE;
                                // on "rassure" la fonction qui nous a appele
                                // car il n'y a plus rien a faire plus haut
                        }
                    default: 
                        cerr << "ERREUR INTERNE dans mauvais resultat d'appel"
                             << "recursif de ProcInfo::ProcInstruction::"
                             << "doOneStepAndAdvancePC()...\n";
                        exit(1);
                } // switch(incrementation dans le body)
            }
                break; // redondance, on n'arrive jamais ici 
            case COND_EVAL_FALSE:
                crtInstr -> condEval = COND_NOT_EVAL;// reset pour le prochain
                // coup, lors d'un autre passage (par exemple si on est
                // englobe dans un WHILE plus grand)
                return ADV_REACHED_END;  // pareil, on le delegue plus haut
            default:
                cerr << "ERREUR INTERNE valeur illegale " 
                     << crtInstr -> condEval << "  dans "
                     << "ProcInfo::ProcInstruction::"
                     << "doOneStepAndAdvancePC()...\n";
                exit(1);
        } // switch(crtInstr -> condEval)
        cerr << "ERREUR INTERNE probleme de coherence " 
             << " dans ProcInfo::ProcInstruction::"
             << "doOneStepAndAdvancePC()...\n";
        exit(1);                
    } // doOneStepAndAdvancePC()
    
    // et maintenant la partie ordonnanceur, avec seulement le tourniquet 
    // pratiquement la meme que celle vue en TP, a ceci pres que le tourniquet
    // a maintenant un peu d'aleatoire : de temps a autre il change d'avis et
    // ne prend pas le tout premier processus, mais un autre.
    
    int bRand(int upLimit) {
        return (1 + static_cast<int>(static_cast<double>(upLimit)*
                                     rand()/(RAND_MAX+1.0)));
    }
    
    
    void Scheduler::enQueueProc(const int procPid) {
        waitQueue . push_back(procPid); // par derriere
    }
    
    void Scheduler::enQueueAllProc() {
        for(unsigned int kProc = 0; kProc < pInfo -> procData . size(); 
            ++kProc) {
            if(pInfo -> procData [kProc] -> procStatus 
               == ProcInfo::STAT_WAITING) {
                enQueueProc(kProc);
            }
        }
    }
    
    void Scheduler::displayQueue(ostream *s) {
        (*s) << "(" ;
        for(unsigned int kProc = 0; kProc < waitQueue . size(); ++kProc) {
            (*s) << waitQueue[kProc] ;
            if(kProc < waitQueue . size() - 1) (*s) << ",";
        }
        (*s) << ")\n";
    }
    
    // la fonction "principale" de l'ordonnanceur
    
    int Scheduler::electAProc() {
        int chosenProc (ProcInfo::invalidProcPid);
        // le tourniquet est la seule politique : 
        // on sort par devant (et on sait que
        // enQueueProc fait rentrer par derriere)
        int siJamais (bRand(5)); // enfin, presque...
        if(waitQueue . size()) {
            if(siJamais >= 4 && waitQueue . size() > 1) {
                    chosenProc = waitQueue[1]; // il y a des jours comme ca
                    int pasCetteFois  = waitQueue . front();
                    waitQueue . pop_front(); // pour rentrer derriere
                    waitQueue . pop_front(); // car c'est l'elu cette fois
                    waitQueue . push_back(pasCetteFois); // c'est reparti
            }
            else { // comme d'hab
                chosenProc = waitQueue . front();
                waitQueue . pop_front();
            }
        }
        if(qSchedulingVerbose) {
            cerr << "Sched: elected " << chosenProc << " remaining ";
            displayQueue(&cerr);
        }
        return chosenProc;
    } // electAProc()

} // namespace ProcDebug

#undef STATUS

