/**
 *
 * @File : ProcDebug.hxx
 *
 * @Author : A. B. Dragut
 *
 * @Date : 28/11/2011
 *
 * @Synopsis : debugger-emulateur
 *
 **/


#ifndef __PROCDEBUG_HXX__
#define __PROCDEBUG_HXX__
#include <vector> 
#include <deque> 
#include <string> 
#include <map> 

namespace ProcDebug {
  
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

#endif /*  __PROCDEBUG_HXX__ */
