/**
 *
 * @File : proj.cxx
 *
 * @Date : 18/12/2011
 *
 *
 * @Synopsis : main() de debugger-simulateur
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

#include "MiniDbg.h"
#include "ProcDebug.h"
#include "CExc.h"
#include "nsSysteme.h"

using namespace nsFctShell; // DerouterSignaux()
using namespace nsSysteme;
using namespace ProcDebug;
using namespace std;

namespace
{
    // Déclarées en globale car on en a besoin dans le handler à qui
    // on ne peux pas passer de paramètres.
    ProcInfo * procInfo;
    int newProc2Run;
    MiniDbg * miniDbg;

    void LancerDbg (int n)
    {
        if (procInfo -> procData[newProc2Run] -> procStatus !=
                                            ProcInfo::STAT_TERMINATED)
            procInfo -> updateProcData (newProc2Run, ProcInfo::START_TRACE);

        // Création du débugger avec une condition
        // pour qu'il ne soit initialisé qu'une fois
        // (avec le processus dont l'instruction était en cours lors 
        // de la réception du signal SIGQUIT)
        if (!miniDbg)
            miniDbg = new MiniDbg (procInfo, newProc2Run);
        else
            miniDbg -> SetProc (newProc2Run);

        cout << "Interruption à la ligne "
             << procInfo -> procData[newProc2Run] -> nextLineNumber -1
             << '\n';

        miniDbg -> Prompt();

    } // LancerDbg()

    void TraiterSig (int n)
    {
        int processus = (miniDbg ? miniDbg -> GetProc() : newProc2Run);
        if (procInfo -> procData[processus] -> hanDler &&
            Sigismember(&procInfo -> procData[processus] -> sigMask, n))
        {
            // une par une les instructions du traitant
            for(vector<ProcInfo::ProcInstruction*>::iterator i (
                    procInfo -> procData[processus] -> hanDler ->
                        bodyInstr . begin());
                i < procInfo -> procData[processus] -> hanDler ->
                         bodyInstr . end();
                ++i)
                procInfo -> avancerDUnPas(processus, *i);

        }
        else
        {
            Signal(n, SIG_DFL);
            raise (n);
        }

    } // TraiterSig()

} // namespace anonyme

int main (int argc, char *argv[])  {
    try {
        int reqVerb (0);
        const int nbVerb(5);
        int verbLevel[nbVerb];
        if(argc != 3                     || 
           (reqVerb = atoi(argv[2])) < 0 || 
           reqVerb > nbVerb)
        {
               throw CExc ("main()", string("Usage : ") +  argv[0] + 
                " <list of prog file paths>\n"
                "                    <verboseLevel(from 0 to 5, where\n"
                "        1 for main steps, 2 + tokens, 3 + parsing,\n"
                "        4 + exec and 5 + sched)>\n" + 
                "Example: " + argv[0] + " 'tst/tst1.0.m tst/tst1.1.m' 5\n");
        }

        int k (0);
        for(; k < reqVerb; ++k)
        {
            verbLevel[k] = 1;
        }
        for(; k < nbVerb;  ++k)
        {
            verbLevel[k] = 0;
        }

        procInfo = new ProcInfo(argv[1],
                        verbLevel[0],verbLevel[1],
                        verbLevel[2],verbLevel[3]);
        Scheduler scheduler(procInfo,verbLevel[4]);
        procInfo -> scheduler = &scheduler;
        scheduler . enQueueAllProc();
        if(verbLevel[1])
        {
            cerr << "Starting up...";
        }

        /**
         * C'est ici que tout ce passe !
         * 
         * Déroulement normal :
         * Tant qu'on a des programmes non terminés
         * (outstandingProcCount est décrémenté à chaque fin de processus)
         * On exécute une nouvelle instruction, et à chaque fois,
         * on éli un processus (si on a qu'un seul mini-programme,
         * ce sera toujours le même)
         * Pour qu'un processus soit élu, il faut qu'il soit mis en queue
         **/

/*
    // Affichage pour pouvoir savoir quel est le status du processus
    // juste en l'affichant et en comparant

        cout << ProcInfo::STAT_WAITING << ' '       // 0
             << ProcInfo::STAT_RUNNING << ' '       // 1
             << ProcInfo::STAT_TRACEEND << ' '      // 8
             << ProcInfo::STAT_TRACESTEPRUN << ' '  // 9
             << ProcInfo::STAT_TERMINATED << endl;  // 7
*/

        DerouterSignaux (TraiterSig); // pour SIGNAL
        Signal (SIGQUIT, LancerDbg);  // Déroutement de SIGQUIT vers LancerDbg

        // a enlever pour la release
        procInfo -> qDoSleepAfterEachInstruction = true;

        while(procInfo -> outstandingProcCount)
        {
            newProc2Run = scheduler . electAProc();
            if(newProc2Run == ProcInfo::invalidProcPid) continue;

            procInfo -> avancerDUnPas(newProc2Run); // voir dans ProcDebug.cxx
        }

        delete procInfo;
        if (miniDbg)
            delete miniDbg;

        return 0;

    } // try

    catch (const CExc & Exc) {
        cerr << Exc << endl;
        return errno;
    }
    catch (const exception & Exc) {
        cerr << "Exception : " << Exc.what () << endl;
        return 1;
    }
    catch (...) {
        cerr << "Exception inconnue recue dans la fonction main()" << endl;
        return 1;
    }

} // main()
