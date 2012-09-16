/**
 *
 * @File : nsSysteme.h
 *
 * @Synopsis : nsSysteme espace de noms qui contient les prototypes des wrappers
 *             des fonctions systeme
 * @Synopsis : nsFctShell espace de noms qui contient les prototypes des wrappers
 *             des mini fonctions shell
 *
 *
 **/
#if !defined __NSSYSTEME_H__
#define      __NSSYSTEME_H__

#include <cstddef>        // size_t
#include <dirent.h>       // DIR * , dirent*
#include <sys/types.h>    // ssize_t                                                    
#include <sys/stat.h>     // struct stat, stat(), fstat()
#include <signal.h>       // struct sigaction, sigaction(), sigset_t
#include <sys/wait.h>    //waitpid()

#include "string.h"      

#include "CExc.h"


/////////////////DEBUT D'ESPACE DE NOMS nsSysteme//////////////////////

//  Declarations des fonctions concernant les fichiers
//  ===================================================================

namespace nsSysteme
{

   void         Stat    (const char * file_name, struct stat * buf)
                             throw (CExc);

    void        Close  (int fd)
                             throw (CExc);


    int         Open   (const char * pathname, int flags)
                             throw (CExc);
    int         Open   (const char * pathname, int flags, ::mode_t mode)
                             throw (CExc);
    std::size_t Read   (int fd, void * buf, std::size_t count)
                             throw (CExc);

    void        Stat   (const char * file_name, struct stat * buf)
                             throw (CExc);

    std::size_t Write  (int fd, const void * buf, std::size_t count)
                             throw (CExc);

    void        Unlink (const char * pathname)
                             throw (CExc);

    void        LStat   (const char * file_name, struct stat * buf)
                             throw (CExc);
    int 	    Dup2 (int oldfd, int newfd)
                            throw (CExc);

    ::off_t     Lseek (int fildes, ::off_t offset, int whence)
    				throw (CExc); 



   
   

    //  Declarations des fonctions concernant les repertoires
    // ===================================================================
    // 

    void        ChDir   (const char * path)
                             throw (CExc);

    void        GetCwd  (char * path, size_t taille)
                             throw (CExc);

    DIR *       OpenDir (const char * dir_name)
                             throw (CExc);

    dirent *    ReadDir (DIR * dirStreamP)
                             throw (CExc);

    void        CloseDir(DIR * dirStreamP)
                             throw (CExc);

    //   Declarations des fonctions concernant les signaux 
    // ===================================================================
    // 


    typedef void (*sighandler_t)(int);

    const int CstSigMax = 32; //_NSIG
    //tableau de traitants de signal

    typedef  struct sigaction TabSigHandlers[_NSIG];

    void         Sigaction   (int signum, 
                              const struct sigaction * act, 
                              struct sigaction * oldact) 
                        throw (CExc); 

    sighandler_t Signal      (int NumSig, sighandler_t NewHandler) 
                        throw (CExc); 
 
    int          Sigemptyset (sigset_t *set)
                        throw (CExc);

    int          Sigaddset   (sigset_t *set, int signum)
                        throw (CExc);

    int          Sigdelset   (sigset_t *set, int signum)
                        throw (CExc);

    int          Sigismember (const sigset_t *set, int signum)
                        throw (CExc);

    void         Sigprocmask (int  how,  const ::sigset_t * set, 
                                               ::sigset_t * oldset) 
                        throw (CExc); 

    void Kill (::pid_t pid, int sig) throw (CExc);

 //   Declarations des fonctions concernant le multiplexage des E/S                                                     
 //  =====================================================================                                                    

     int         Select    (int n, ::fd_set * readfds,
			     ::fd_set * writefds  = 0,
			     ::fd_set * exceptfds = 0,
			     struct timeval * timeout = 0)
	 throw (CExc);



    //   Declarations des fonctions concernant les processus
    // ===================================================================
    // 

    ::pid_t Fork (void) throw (CExc);


 	

    ::pid_t Waitpid (::pid_t pid, int * status  = 0 , 
                                   int options =0 ) 
    throw (CExc);






} // nsSysteme


/////////////////FIN D'ESPACE DE NOMS nsSysteme/////////////////////////////




/////////////////DEBUT D'ESPACE DE NOMS nsFctShell//////////////////////////

//  Declarations des fonctions shell
//  =======================================================================

namespace nsFctShell {
 
  void FileCopy (const char * const Destination,
                   const char * const Source,
                   const size_t       NbBytes,
                   const bool         syn = false)
                  throw (nsSysteme::CExc);




   void Destroy  (const char * const File)  throw (nsSysteme::CExc);


   void DerouterSignaux(sighandler_t Traitant) throw(nsSysteme::CExc);

   void TestFdOuverts (std::ostream & os = std::cout) throw (nsSysteme::CExc); 


} // nsFctShell


/////////////////FIN D'ESPACE DE NOMS nsFctShell////////////////////////////







////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

//  Definitions courtes des fonctions concernant les fichiers
//  =========================================================

inline
void nsSysteme::Stat (const char * file_name, struct stat * buf)
    throw (CExc)
{
    if (::stat (file_name, buf))
        throw CExc ("stat()", std::string("fichier :")+ file_name);

} // Stat()

inline void nsSysteme::Close (int fd) throw (CExc)
{
    if (::close (fd)) throw CExc ("close()", fd);

} // Close()

inline 
std::size_t nsSysteme::Read (int fd, void * buf, std::size_t count)
    throw (CExc)
{
    ::ssize_t Res;
    if (-1 == (Res = ::read (fd, buf, count)))
        throw CExc ("read()", fd);

    return Res;

} // Read()

inline std::size_t nsSysteme::Write (int fd, const void * buf,
                                     std::size_t count)
    throw (CExc)
{
    ::ssize_t Res;
    if (-1 == (Res = ::write (fd, buf, count)))
        throw CExc ("write()", fd);

    return Res;

} // Write()

inline 
void nsSysteme::Unlink (const char * pathname)
    throw (CExc)
{
    if (::unlink (pathname))
        throw CExc ("unlink()", pathname);

} // Unlink()

inline
void nsSysteme::LStat (const char * file_name, struct stat * buf)
    throw (CExc)
{
    if (::lstat (file_name, buf))
        throw CExc ("lstat()", std::string("fichier :") + file_name);

} // LStat()



inline int nsSysteme::Dup2 (int oldfd, int newfd)
    throw (CExc)
{
    if (-1 == ::dup2 (oldfd, newfd))
        throw CExc ("dup2()", oldfd);

    return newfd;

} // Dup2()


inline ::off_t nsSysteme::Lseek (int fildes, ::off_t offset, int whence)
    throw (CExc) 
{ 
    ::off_t Res; 
    if (-1 == (Res = ::lseek (fildes, offset, whence))) 
        throw CExc ("lseek()", fildes); 

    return Res; 

} // Lseek() 




//
//  Definitions courtes des fonctions concernant les repertoires
//  ============================================================

inline void nsSysteme::ChDir(const char * path)
  throw (CExc) 
{
  if(::chdir(path))
    throw CExc ("chdir()", path);
} // ChDir()

inline void nsSysteme::GetCwd(char * path, size_t taille)
  throw (CExc) 
{
  if(::getcwd(path, taille) == 0)
    throw CExc ("getcwd()", path);
} // GetCwd()

inline DIR *nsSysteme::OpenDir(const char * dir_name)
  throw (CExc) 
{
  DIR *pDir;
  if((pDir = ::opendir (dir_name)) == 0) 
        throw CExc ("dir()", dir_name);
  return pDir;
} // Opendir()

inline dirent * nsSysteme::ReadDir(DIR * dirStreamP)
        throw (CExc) 
{
  errno = 0;
  dirent * pEntry (::readdir(dirStreamP));

  if(pEntry == 0 && errno)
    throw CExc ("readdir()", "");

  return(pEntry);

} // ReadDir()

inline void  nsSysteme::CloseDir(DIR * dirStreamP) 
     throw (CExc) 
{
  if(::closedir(dirStreamP)) 
    throw CExc ("closedir()", "");

} // CloseDir()



// =============================================================================
//  Definitions courtes des fonctions concernant les signaux 
//  ============================================================================

inline void nsSysteme::Sigaction (int signum, 
                                  const struct sigaction * act, 
                                  struct sigaction * oldact) 
    throw (CExc) 
{ 
    if (::sigaction (signum, act, oldact)) 
        throw CExc ("sigaction()",""); 

} // Sigaction() 

inline int nsSysteme::Sigemptyset (sigset_t *set)
    throw (CExc)
{
    if (::sigemptyset (set))
        throw CExc ("sigemptyset()", "");

    return 0;

} // Sigemptyset()

inline int nsSysteme::Sigaddset (sigset_t *set, int signum)
    throw (CExc)
{
    if (::sigaddset (set, signum))
        throw CExc ("sigaddset()", "");

    return 0;

} // Sigaddset()

inline int nsSysteme::Sigdelset (sigset_t *set, int signum)
    throw (CExc)
{
    if (::sigdelset (set, signum))
        throw CExc ("sigdelset()", "");

    return 0;

} // Sigdelset

inline int nsSysteme::Sigismember (const sigset_t *set, int signum)
    throw (CExc)
{
    int Res;
    if (-1 == (Res = ::sigismember (set, signum)))
        throw CExc ("sigismember()", "");

    return Res;

} // Sigismember()

inline void nsSysteme::Sigprocmask (int   how,  
                                    const ::sigset_t * set, 
                                          ::sigset_t * oldset) 
    throw (CExc) 
{ 
    if (::sigprocmask (how, set, oldset))
        throw CExc ("sigprocmask()",""); 

} // Sigprocmask() 


inline void nsSysteme::Kill (::pid_t pid, int sig) throw (CExc) 
{ 
    if (::kill (pid, sig)) throw CExc ("kill()", sig); 

} // Kill() 


//
//  Definitions courte d'une fonction concernant le multiplexage des E/S                                                     
//  ==========================================================================                                                     

 inline int   nsSysteme::Select    (int n, ::fd_set * readfds,
			     ::fd_set * writefds  /*= 0*/,
			     ::fd_set * exceptfds /*= 0*/,
			     struct timeval * timeout /*= 0*/)
		 throw (CExc) {

    int NbEvent;
    if (-1 == (NbEvent = ::select (n,
                                   readfds, writefds,  exceptfds,
                                   static_cast <timeval *> (timeout))))
        throw CExc ("select()","");

    return NbEvent;

} // Select()      


//
//  Definitions courtes des fonctions concernant les processus                                                     
//  ==========================================================================                                                     



inline ::pid_t nsSysteme::Fork (void) throw (CExc) 
{ 
    ::pid_t PidFils; 
    if  (-1 == (PidFils = ::fork ())) throw CExc ("fork()",PidFils); 

    return PidFils; 

} // Fork() 


inline ::pid_t nsSysteme::Waitpid (::pid_t pid, int * status /* = 0 */, 
                                   int options /* = 0 */) 
    throw (CExc) 
{ 
    ::pid_t pidRes; 
    //waipid peut revenir avec EINTR si un SIGCHLD est recu et WNOHANG n'est pas positionne
    if ((-1 == (pidRes = ::waitpid (pid, status, options)))&&(0==options)&&(EINTR!=errno)) 
        throw CExc ("waitpid()",pid); 

    return pidRes; 
}
                                                                               


#endif    /* __NSSYSTEME_H__ */
