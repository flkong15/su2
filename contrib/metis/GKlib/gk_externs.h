/*!
\file gk_externs.h
\brief This file contains definitions of external variables created by GKlib

\date   Started 3/27/2007
\author George
\version\verbatim $Id: gk_externs.h 10711 2011-08-31 22:23:04Z karypis $ \endverbatim
*/

#ifndef _GK_EXTERNS_H_
#define _GK_EXTERNS_H_

/*************************************************************************
* Extern variable definition. Hopefully, the __thread makes them thread-safe.
**************************************************************************/
#ifndef _GK_ERROR_C_

/* declared in error.c */
#ifdef TLS
extern TLS int gk_cur_jbufs;
extern TLS jmp_buf gk_jbufs[];
extern TLS jmp_buf gk_jbuf;
#else
extern int gk_cur_jbufs;
extern jmp_buf gk_jbufs[];
extern jmp_buf gk_jbuf;
#endif // TLS

#endif

#endif
