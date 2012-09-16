/**
 *
 * @File : MiniDbg.cxx
 *
 * @Date : 18/12/2011
 *
 * @Synopsis : Définition de la classe MiniDbg
 * 
 **/

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

#include "MiniDbg.h"
#include "nsSysteme.h"

using namespace std;
using namespace nsSysteme;
using namespace ProcDebug;

// À afficher si on essaye d'exécuter une commande sur un processus terminé
#define PROCENDED "Processus déjà terminé\n"

// idem que dans ProcDebug.cxx
#define STATUS procData[m_Proc] -> procStatus

namespace ProcDebug
{

    MiniDbg::MiniDbg (ProcInfo * procInfo, int Proc) throw()
    : m_ProcInfo (procInfo), m_Proc (Proc), m_GoOut (false) {}

    void MiniDbg::Prompt (void) throw ()
    {
        for (string Str; ; )
        {
            string Prompt (m_ProcInfo -> procData[m_Proc] -> progName +
                           ":mDbg> ");
            cout << Prompt;

            getline (cin, Str); // Comme ça, si on tape entrer sans rien,
                                // on reviens, contrairement à
                                // si on avait utilisé cin >> 

            if (Str.empty())
            {
                // au cas ou un petit c** ferais ctrl+d
                if (cin.eof())
                {
                    cout << '\n'; // uniquement si eof, car sinon
                                  // on l'a déjà fait avec getline
                    cin.clear();
                }
                continue;
            }

            // On met la chaine en minuscule au cas ou...
            transform(Str.begin(), Str.end(),
                      Str.begin(), (int (*) (int))tolower);
                // Le troisième paramètre prend un pointeur de fonction
                // de type int fct (int i), c'est le cas de tolower
                // mais apparemment le compilateur ne le vois pas...

            m_Cmd.clear(); // sinon les commandes s'accumulent...

            // Pour que les blancs soient ignorés, met la ligne lue
            // dans le vecteur en passant par un istringstream
            // c'est mieu que d'utiliser strtok avec " " comme séparateur
            // (nb: on recycle Str pour la lecture...)

            for (istringstream istr (Str); ! (istr >> Str).eof(); )
                m_Cmd.push_back(Str);
            m_Cmd.push_back(Str); // Lorsqu'on éjecte la dernière string,
                                  // le flux est eof, et on sort sans
                                  // l'avoir ajouté :(

            if      (m_Cmd[0] == "continue"  ) GererContinue();
            else if (m_Cmd[0] == "step"      ) GererStep();
            else if (m_Cmd[0] == "print"     ) GererPrint();
            else if (m_Cmd[0] == "display"   ) GererDisplay();
            else if (m_Cmd[0] == "modify"    ) GererModify();
            else if (m_Cmd[0] == "break"     ) GererBreak();
            else if (m_Cmd[0] == "show"      ) GererShow();
            else if (m_Cmd[0] == "remove"    ) GererRemove();
            else if (m_Cmd[0] == "stop"      ) GererStop();
            else if (m_Cmd[0] == "start"     ) GererStart();
            else if (m_Cmd[0] == "restart"   ) GererRestart();
            else if (m_Cmd[0] == "changeproc") GererChangeProc();
            else if (m_Cmd[0] == "status"    ) GererStatus();
            else if (m_Cmd[0] == "end"       )
            {
                if (-1 != GererEnd()) return;
            }
            else if (m_Cmd[0] == "quit"      )
            {
                if (-1 != GererQuit()) return;
            }
            else cout << "Commande inconnue\n";

            if (m_GoOut) return;
            // indispensable, car par exemple : on lance le programme,
            // puis on lance le dégugger et on tape "continue", puis on
            // relance le débugger, sur ce prompt, si on tape end (ou quit),
            // on sortira du deuxième traitant (prompt), et on reviendra
            // dans le premier qui s'était arrêté au milieu d'un continue,
            // c'est pour ça qu'on test si on dois sortir ici et dans
            // GererContinue()

        }

    } // Prompt()

    void MiniDbg::GererContinue () throw ()
    {
        if (1 != m_Cmd.size())
        {
            cout << "Usage : continue\n";
            return;
        }

        if (m_ProcInfo -> STATUS == ProcInfo::STAT_TERMINATED)
        {
            cout << PROCENDED;
            return;
        }

        cout << "Reprise à la ligne "
             << m_ProcInfo -> procData[m_Proc] -> nextLineNumber -1
             << '\n';

        // On cherche dans m_Break le prochain point d'interruption
        // pour ne pas avoir à parcourir tout le vecteur à chaque
        // fois qu'on avance d'une instruction.
        // Et puis, on a qu'un seul break par un continue...

        int Breakpoint (0); // initialisé à zero car si on a aucun break,
                            // on ne s'arretera pas (la ligne 0 n'existe pas)
        for (vector<int>::iterator i (m_Break.begin());
             i < m_Break.end(); ++i)
            if (*i >= m_ProcInfo -> findCrtInstruction(
                    m_ProcInfo -> procData[m_Proc] -> proGram) -> lineNumber)
            {
                Breakpoint = *i;
                break;
            }

        // Et on continue uniquement le processs tracé

        m_ProcInfo -> avancerDUnPas(m_Proc); // car sinon on reste bloqué
                                             // à cause du >=
        for (; m_ProcInfo -> STATUS != ProcInfo::STAT_TERMINATED; )
        {
            if (m_GoOut) return;
            if (Breakpoint == m_ProcInfo -> findCrtInstruction(
                m_ProcInfo -> procData[m_Proc] -> proGram) -> lineNumber)
            {
                cout << "\nBreakpoint à la ligne " << Breakpoint << '\n';
                return;
            }

            m_ProcInfo -> avancerDUnPas(m_Proc);
        }

    } // GererContinue()

    void MiniDbg::GererStep () throw ()
    {
        if (1 != m_Cmd.size())
        {
            cout << "Usage : step\n";
            return;
        }

        if (m_ProcInfo -> STATUS == ProcInfo::STAT_TERMINATED)
        {
            cout << PROCENDED;
            return;
        }

        cout << "Execution de la ligne n° "
             << m_ProcInfo -> procData[m_Proc] -> nextLineNumber
             << '\n';
        m_ProcInfo -> avancerDUnPas (m_Proc);

        for (vector<string>::iterator i (m_VarAAfficher.begin());
             i < m_VarAAfficher.end(); ++i)
            AfficherVar(*i);

    } // GererStep()

    void MiniDbg::GererPrint () throw ()
    {
        if (2 != m_Cmd.size())
        {
            cout << "Usage : print <nom_variable>\n";
            return;
        }

        AfficherVar (m_Cmd[1]);

    } // GererPrint()

    void MiniDbg::GererDisplay () throw ()
    {
        if (2 != m_Cmd.size())
        {
            cout << "Usage : print <nom_variable>\n";
            return;
        }

        if (! AfficherVar(m_Cmd[1])) // AfficherVar renvoie 0 si on a réussi
            m_VarAAfficher.push_back(m_Cmd[1]);

    } // GererDisplay()

    void MiniDbg::GererModify () throw () // enfin une fonction utile...
    {
        if (3 != m_Cmd.size())
        {
            cout << "Usage : modifiy <variable> <valeur>\n";
            return;
        }

        int Val;
        {
            istringstream istr (m_Cmd[2]);
            istr >> Val;
            if (istr.fail())
            {
                cout << "Usage : modifiy <variable> <valeur>\n";
                return;
            }
        }

        // l'indice du symbole contenant Var dans symbolTable
        int Indice (m_ProcInfo -> procData[m_Proc]
                        -> findExistentSymbol(m_Cmd[1]));

        if (-1 == Indice)
        {
            cerr << "Variable introuvable\n";
            return;
        }

        m_ProcInfo -> procData[m_Proc] -> symbolTable[Indice] . value = Val;

        cout << m_ProcInfo -> procData[m_Proc] -> symbolTable[Indice] . varIdent
             << " = "
             << m_ProcInfo -> procData[m_Proc] -> symbolTable[Indice] . value
             << '\n';

    } // GererModify()

    void MiniDbg::GererBreak () throw ()
    {
        // Beaucoups de tests avant d'ajouter le breakpoint au vecteur...

        if (2 != m_Cmd.size())
        {
            cout << "Usage : break <numero_ligne>\n";
            return;
        }

        unsigned numLigne;

        {
            istringstream istr (m_Cmd[1]);
            istr >> numLigne;
            if (istr.fail())
            {
                cout << "Usage : break <numero_ligne>\n";
                return;
            }
        }

        // Comptage des lignes du fichier
        unsigned maxNbLignes (0);
        ifstream fic (m_ProcInfo -> procData[m_Proc] -> progName .c_str());
        // en une seule ligne s'il vous plaît ^_^
        for (string Str; ! getline(fic, Str).eof(); ++maxNbLignes);
        fic.close();

        if (0 >= numLigne || numLigne > maxNbLignes)
        {
            cout << "Numero de ligne invalide\n";
            return;
        }

        // On vérifie si le breakpoint n'existe pas déjà
        for (vector<int>::iterator i (m_Break.begin());
             i < m_Break.end(); ++i)
            if ((int) numLigne == *i)
            {
                cout << "Breakpoint déjà enregistré\n";
                return;
            }

        if ((int)numLigne == m_ProcInfo -> findCrtInstruction (
                  m_ProcInfo -> procData[m_Proc] -> proGram) -> lineNumber)
            cout << "Ce breakpoint pointe sur la ligne suivante "
                 << "qui est déjà interrompue !\nIl sera donc ignoré "
                 << "pour cette exécution\n";


        // pfiou...
        m_Break.push_back(numLigne);
        cout << '[' << m_Break.size() << "] ligne n° " << numLigne << '\n';
        sort(m_Break.begin(), m_Break.end()); // histoire de

    } // GererBreak()

    void MiniDbg::GererShow () throw ()
    {
        if (2 != m_Cmd.size())
        {
            cout << "Usage : show <quoi>\n";
            return;
        }

        if (m_Cmd[1] == "display")
        {
            for (unsigned i(0); i < m_VarAAfficher.size(); ++i)
            {
                cout << '[' << i+1 << "] "; // numéro, celui à spécifier
                                            // si on veux supprimer
                AfficherVar(m_VarAAfficher[i]);
            }
        }
        else if (m_Cmd[1] == "break")
        {
            for (unsigned i(0); i < m_Break.size(); ++i)
                cout << '[' << i+1 << "] ligne n° " << m_Break[i] << '\n';
        }
        else if (m_Cmd[1] == "proc")
        {
            for (unsigned i(0); i < m_ProcInfo->procData.size(); ++i)
                cout << '[' << i+1 << "] "
                     << m_ProcInfo -> procData[i] -> progName << endl;
        }
        else cout << "<quoi> invalide\n";

    } // GererShow()

    void MiniDbg::GererRemove () throw ()
    {
        if (3 != m_Cmd.size())
        {
            cout << "Usage : remove <quoi> <n°>\n";
            return;
        }

        unsigned Num;
        {
            istringstream istr (m_Cmd[2]);
            istr >> Num;
        }
        --Num; // parce que le "numéro" porté par le display est l'indice
               // dans le vecteur - 1
               // (le n° 1 est l'élement à la position 0 dans le vecteur)

        if (m_Cmd[1] == "display")
        {
            if (Num >= m_VarAAfficher.size())
            {
                cout << "Indice incorrect\n";
                return;
            }

            cout << "Suppression de la variable \""
                 << *(m_VarAAfficher.begin() + Num)
                 << "\" à afficher\n";
            m_VarAAfficher.erase(m_VarAAfficher.begin() + Num);
        }
        else if (m_Cmd[1] == "break")
        {
            if (Num >= m_Break.size())
            {
                cout << "Indice incorrect\n";
                return;
            }

            cout << "Suppression du breakpoint [" << Num+1 // a cause du --Num
                 << "] à la ligne n° "
                 << *(m_Break.begin() + Num) << '\n';
            m_Break.erase(m_Break.begin() + Num);
        }
        else cout << "<quoi> invalide\n";

    } // GererRemove()

    void MiniDbg::GererStop () throw ()
    {
        if (m_ProcInfo -> STATUS == ProcInfo::STAT_TERMINATED)
        {
            cout << PROCENDED;
            return;
        }

        m_ProcInfo -> updateProcData (m_Proc, ProcInfo::PROC_TERM);

    } // GererStop

    void MiniDbg::GererStart () throw ()
    {
        if (2 < m_Cmd.size())
        {
            cout << "Usage : start <step | >\n";
            return;
        }
        else
            if (2 == m_Cmd.size() && m_Cmd[1] != "step")
            {
                cout << "Usage : start <step | >\n";
                return;
            }

        if (m_ProcInfo -> STATUS != ProcInfo::STAT_TERMINATED)
        {
            cout << "Processus non terminé\n";
            return;
        }

        // "redémarrage" du processus
        // on déréférence car sinon, lorsque le programme va avancer,
        // il modifiera proGramInit qui deviendra obsolète
        *m_ProcInfo -> procData[m_Proc] -> proGram =
                            *m_ProcInfo -> procData[m_Proc] -> proGramInit;
        m_ProcInfo -> procData[m_Proc] -> nextLineNumber = 1;
        m_ProcInfo -> STATUS = ProcInfo::STAT_TRACEEND;
        ++m_ProcInfo -> outstandingProcCount;

        if (m_Cmd[1] == "step")
        {
            // il faut enlever le "step" dans "start step" parce que
            // GererStep() veut que m_Cmd ne contienne qu'un élément
            // (il nous fait confiance, il ne vérifie pas si c'est bien step)
            m_Cmd.erase(m_Cmd.begin() +1);
            GererStep();
        }
        else GererContinue();

    } // GererStart()

    void MiniDbg::GererRestart () throw ()
    {
        if (2 < m_Cmd.size())
        {
            cout << "Usage : start <step | >\n";
            return;
        }
        else
            if (2 == m_Cmd.size() && m_Cmd[1] != "step")
            {
                cout << "Usage : start <step | >\n";
                return;
            }

        GererStop();
        GererStart();

    } // GererRestart()

    void MiniDbg::GererChangeProc () throw ()
    {
        if (2 != m_Cmd.size())
        {
            cout << "Usage : changeproc <pid>\n";
            return;
        }

        unsigned numProc;
        {
            istringstream istr (m_Cmd[1]);
            istr >> numProc;
            if (istr.fail())
            {
                cout << "Usage : changeproc <pid>\n";
                return;
            }
        }
        --numProc; // Pour les mêmes raisons que avant...

        if (numProc >= m_ProcInfo -> procData.size() ||
            numProc < 0)
        {
            cout << "Indice invalide\n";
            return;
        }

        m_Proc = numProc;
        if (m_ProcInfo -> STATUS != ProcInfo::STAT_TRACEEND &&
            m_ProcInfo -> STATUS != ProcInfo::STAT_TERMINATED)
            m_ProcInfo -> updateProcData(m_Proc, ProcInfo::START_TRACE);

    } // GererChangeProc()

    void MiniDbg::GererStatus () throw ()
    {
        if (1 != m_Cmd.size())
        {
            cout << "Usage : status\n";
            return;
        }

        // Normalement, on ne peux avoir que
        // WAITING, TRACEEND et TRACETERMINATED,
        // mais cette fonction est utile pour le débuggage du débugger...
        switch (m_ProcInfo -> procData[m_Proc] -> procStatus)
        {
          case ProcInfo::STAT_WAITING :
            cout << "STAT_WAITING\n";
            break;
          case ProcInfo::STAT_RUNNING :
            cout << "STAT_RUNNING\n";
            break;
          case ProcInfo::STAT_IOWAIT :
            cout << "STAT_IOWAIT\n";
            break;
          case ProcInfo::STAT_MUTEXWAIT :
            cout << "STAT_MUTEXWAIT\n";
            break;
          case ProcInfo::STAT_MUTEXGRAB :
            cout << "STAT_MUTEXGRAB\n";
            break;
          case ProcInfo::STAT_NOMUTEX :
            cout << "STAT_NOMUTEX\n";
            break;
          case ProcInfo::STAT_SYS :
            cout << "STAT_SYS\n";
            break;
          case ProcInfo::STAT_TERMINATED :
            cout << "STAT_TERMINATED\n";
            break;
          case ProcInfo::STAT_TRACEEND :
            cout << "STAT_TRACEEND\n";
            break;
          case ProcInfo::STAT_TRACESTEPRUN :
            cout << "STAT_TRACESTEPRUN\n";
            break;
          default :
            cout << "Oops\n";
        }

    } // GererStatus()

    int MiniDbg::GererQuit () throw ()
    {
        if (1 != m_Cmd.size())
        {
            cout << "Usage : quit\n";
            return -1;
        }

        // On termine proprement les processus avant de sortir
        for (unsigned i(0); m_ProcInfo -> outstandingProcCount; ++i)
            if (m_ProcInfo -> procData[i] -> procStatus !=
                                            ProcInfo::STAT_TERMINATED)
                m_ProcInfo -> updateProcData (i, ProcInfo::PROC_TERM);
        // (nb : si le processus n'a pas été tracé
        // (avec SIGQUIT ou changeproc),
        // on n'affichera pas qu'il est terminé);

        m_GoOut = true;

        return 0; // tout c'est bien passé

    } // GererQuit()

    int MiniDbg::GererEnd () throw ()
    {
        if (1 != m_Cmd.size())
        {
            cout << "Usage : end\n";
            return -1;
        }

        // on "détrace" tous les processus avant de sortir
        for (unsigned i(0); i < m_ProcInfo -> procData.size(); ++i)
            if (m_ProcInfo -> procData[i] -> procStatus !=
                                                ProcInfo::STAT_TERMINATED)
                m_ProcInfo -> updateProcData(i, ProcInfo::END_TRACE);

        m_GoOut = true;

        return 0;

    } // GererEnd()

    int MiniDbg::AfficherVar (const std::string & Var) throw ()
    {
        // l'indice du symbole contenant Var dans symbolTable
        int Indice (m_ProcInfo -> procData[m_Proc]
                        -> findExistentSymbol(Var));

        if (-1 == Indice)
        {
            cerr << "Variable introuvable\n";
            return -1;
        }

        cout << m_ProcInfo -> procData[m_Proc] -> symbolTable[Indice] . varIdent
             << " = "
             << m_ProcInfo -> procData[m_Proc] -> symbolTable[Indice] . value
             << '\n';

        return 0; // on a réussi a afficher

    } // AfficherVar()

    void MiniDbg::SetProc (int Proc) throw ()
    {
        m_Proc = Proc;

    } // SetProc()

    int MiniDbg::GetProc (void) throw ()
    {
        return m_Proc;

    } // GetProc()

} // ProcDebug

#undef PROCENDED
#undef STATUS

