/**
 *
 * @File : MiniDbg.h
 *
 * @Date : 18/12/2011
 *
 * @Synopsis : Déclarations de la classe MiniDbg
 * 
 **/

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ProcDebug.h"

#ifndef __MINIDBG_H__
#define __MINIDBG_H__

namespace ProcDebug
{
    class MiniDbg
    {
        // Obligatoire pour la manipulation du processus tracé.
        ProcInfo * m_ProcInfo;
        int m_Proc;

        // On choisi un vecteur de string pour la commande,
        // on aurais pu utiliser un istream et éjecter du flux
        // lorsqu'on en a besoin, mais un vecteur apparemment moins gros
        // (e.g : pour une commande de 3 mots, le istream prend 188 octets
        // tandis que le vecteur n'en prend que 12)
        std::vector<std::string> m_Cmd;
        std::vector<std::string> m_VarAAfficher; // à chaque step
        std::vector<int>         m_Break; // devrait être unsigned mais dans
                                          // ProcDebug les lignes sont int

        bool m_GoOut; // pour le end et le quit,
                      // si on a envoyé plusieurs fois SIGQUIT,

      public :
        MiniDbg              (ProcInfo *, int)         throw ();

        ~MiniDbg () {}

        void SetProc         (int Proc)                throw ();
        int  GetProc         (void)                    throw ();
        void Prompt          (void)                    throw ();

      private :

        void GererContinue   (void)                    throw ();
        void GererStep       (void)                    throw ();
        void GererPrint      (void)                    throw ();
        void GererDisplay    (void)                    throw ();
        void GererModify     (void)                    throw ();
        void GererBreak      (void)                    throw ();
        void GererShow       (void)                    throw ();
        void GererRemove     (void)                    throw ();
        void GererStop       (void)                    throw ();
        void GererStart      (void)                    throw ();
        void GererRestart    (void)                    throw ();
        void GererChangeProc (void)                    throw ();
        void GererStatus     (void)                    throw ();
        int  GererEnd        (void)                    throw ();
        int  GererQuit       (void)                    throw ();

        int AfficherVar      (const std::string & Var) throw ();

    }; // MiniDbg

} // namespace ProcDebug

#endif /* __MINIDBG_H__ */
