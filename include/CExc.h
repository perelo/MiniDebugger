/**
 *
 * @File : CExc.h
 * @Author : A. B. Dragut 2011
 *
 * @Synopsis : La classe des exceptions systemes
 *
 **/
#if !defined __CEXC_H__
#define      __CEXC_H__

#include <exception>
#include <string>
#include <iostream>
#include <cerrno>
#include "string.h"

//===============================
// Declaration de la Classe CExc
//===============================
namespace nsSysteme
{
    class CExc        : public std::exception
    {
      protected :
          std::string m_info;
          std::string m_nomf;
          int         m_descrfic;
          bool        m_qdescrfic;
          int         m_errnoval;
             std::string m_strerrorval;
      protected :
          std::ostream & _Edit (std::ostream & os) const;
          
      public :
          CExc (const std::string & NomFonction, 
        	const std::string & Info) throw ();
          CExc (const std::string & NomFonction,
        	int                 Descrfic) throw ();
          virtual ~CExc (void)                            throw ();
          friend std::ostream & operator << (std::ostream & os, 
                                           const CExc & Item);
   
          
    
    }; // CExc
    std::ostream & operator << (std::ostream & os, 
                                const CExc & Item);
   
} // nsSysteme
    

//=============================
// Definition de la Classe CExc
//=============================

#define CEX nsSysteme::CExc
//
// avec inline on peut introduire dans les .h des definitions courtes
//
inline 
CEX::CExc 
(const std::string & NomFonction,
 const std::string & Info) throw ()
    : m_info (Info), m_nomf(NomFonction), m_descrfic(-1), m_qdescrfic(false), 
      m_errnoval(errno),   m_strerrorval(strerror(errno))   {}

inline 
CEX::CExc 
(const std::string & NomFonction,
 int Descrfic) throw ()
    : m_nomf(NomFonction), m_descrfic (Descrfic), m_qdescrfic(true), 
      m_errnoval(errno),   m_strerrorval(strerror(errno)) {}

inline CEX::~CExc (void) throw () {} 



inline 
std::ostream & CEX::_Edit (std::ostream & os) const
{
    os<<"\nNom de la fonction : " <<m_nomf
      <<"\nNo. erreur systeme: "<< m_errnoval
      <<"\ni.e.: "<<std::string("  ")+ m_strerrorval
      <<"\nParametres au moment de l'erreur"
      <<"\n";
    if(m_qdescrfic) {
        os<<"descripteur de l'objet systeme: "<<m_descrfic;
    }
    else {
        os<<m_info;
    }
    os << "\n";
    return os;

} // _Edit()

#undef CEX

inline 
std::ostream & nsSysteme::operator << (std::ostream & os, 
                                    const CExc & Item)
{
    return Item._Edit (os);

} // operator <<




#endif    /* __CEXC_H__ */
