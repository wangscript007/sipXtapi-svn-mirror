//-< QUERY.CPP >-----------------------------------------------------*--------*
// FastDB                    Version 1.0         (c) 1999  GARRET    *     ?  *
// (Main Memory Database Management System)                          *   /\|  *
//                                                                   *  /  \  *
//                          Created:     20-Nov-98    K.A. Knizhnik  * / [] \ *
//                          Last update: 10-Dec-98    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Constructing and hashing database query statements
//-------------------------------------------------------------------*--------*

#define INSIDE_FASTDB

#include "fastdb.h"
#include "symtab.h"
#include "compiler.h"
#include <os/OsDefs.h>

dbQueryElementAllocator dbQueryElementAllocator::instance;


char* dbQueryElement::dump(char* buf)
{
    switch (type) { 
      case qExpression:
	buf += sprintf(buf, " %s ", SIPX_SAFENULL((const char*)ptr));
	break;
      case qVarBool:
	buf += sprintf(buf, "{boolean}");
	break;
      case qVarInt1:
	buf += sprintf(buf, "{int1}");
	break;
      case qVarInt2:
	buf += sprintf(buf, "{int2}");
	break;
      case qVarInt4:
	buf += sprintf(buf, "{int4}");
	break;
      case qVarInt8:
	buf += sprintf(buf, "{int8}");
	break;
      case qVarReal4:
	buf += sprintf(buf, "{real4}");
	break;
      case qVarReal8:
	buf += sprintf(buf, "{real8}");
	break;
      case qVarString:
	buf += sprintf(buf, "{char*}");
	break;
      case qVarStringPtr:
	buf += sprintf(buf, "{char**}");
	break;
      case qVarReference:
	if (ref != NULL) { 
	    buf += sprintf(buf, "{dbReference<%s>}", SIPX_SAFENULL(ref->getName()));
	} else { 
	    buf += sprintf(buf, "{dbAnyReference}");
	}
	break;
      case qVarArrayOfRef:
	if (ref != NULL) { 
	    buf += sprintf(buf, "{dbArray< dbReference<%s> >}", SIPX_SAFENULL(ref->getName()));
	} else { 
	    buf += sprintf(buf, "{dbArray<dbAnyReference>}");
	}
	break;
      case qVarArrayOfRefPtr:
	if (ref != NULL) { 
	    buf += sprintf(buf, "{dbArray< dbReference<%s> >*}", SIPX_SAFENULL(ref->getName()));
	} else { 
	    buf += sprintf(buf, "{dbArray<dbAnyReference>*}");
	}
	break;
      case qVarRawData:
	buf += sprintf(buf, "{raw binary}");
	break;
#ifdef USE_STD_STRING
      case qVarStdString:
	buf += sprintf(buf, "{string}");
	break;
#endif	
    }
    return buf;
}
	

char* dbQueryElement::dumpValues(char* buf)
{
    switch (type) { 
      case qExpression:
	buf += sprintf(buf, " %s ", SIPX_SAFENULL((char*)ptr));
	break;
      case qVarBool:
	buf += sprintf(buf, "%s", *(bool*)ptr ? "true" : "false");
	break;
      case qVarInt1:
	buf += sprintf(buf, "%d", *(int1*)ptr);
	break;
      case qVarInt2:
	buf += sprintf(buf, "%d", *(int2*)ptr);
	break;
      case qVarInt4:
	buf += sprintf(buf, "%d", *(int4*)ptr);
	break;
      case qVarInt8:
	buf += sprintf(buf, INT8_FORMAT, *(db_int8*)ptr);
	break;
      case qVarReal4:
	buf += sprintf(buf, "%f", *(real4*)ptr);
	break;
      case qVarReal8:
	buf += sprintf(buf, "%f", *(real8*)ptr);
	break;
      case qVarString:
	buf += sprintf(buf, "'%s'", SIPX_SAFENULL((char*)ptr));
	break;
      case qVarStringPtr:
	buf += sprintf(buf, "'%s'", SIPX_SAFENULL(*(char**)ptr));
	break;
      case qVarReference:
	if (ref != NULL) { 
	    buf += sprintf(buf, "@%s:%x", SIPX_SAFENULL(ref->getName()), *(oid_t*)ptr);
	} else { 
	    buf += sprintf(buf, "@%x", *(oid_t*)ptr);
	}
	break;
      case qVarArrayOfRef:
	if (ref != NULL) { 
	    buf += sprintf(buf, "{dbArray< dbReference<%s> >}", SIPX_SAFENULL(ref->getName()));
	} else { 
	    buf += sprintf(buf, "{dbArray<dbAnyReference>}");
	}
	break;
      case qVarArrayOfRefPtr:
	if (ref != NULL) { 
	    buf += sprintf(buf, "{dbArray< dbReference<%s> >*}", SIPX_SAFENULL(ref->getName()));
	} else { 
	    buf += sprintf(buf, "{dbArray<dbAnyReference>*}");
	}
	break;
      case qVarRawData:
	buf += sprintf(buf, "{raw binary}");
	break;
#ifdef USE_STD_STRING
      case qVarStdString:
	buf += sprintf(buf, "'%s'", SIPX_SAFENULL(((std::string*)ptr)->c_str()));
	break;
#endif	
    }
    return buf;
}
	
dbQueryElementAllocator::dbQueryElementAllocator() 
: mutex(*new dbMutex), freeChain(NULL) 
{
}

void* dbQueryElementAllocator::allocate(size_t size) 
{ 
    dbCriticalSection cs(mutex);
    dbQueryElement* elem = freeChain;
    if (elem != NULL) {
        freeChain = elem->next;
        return elem;
    } else {
        return dbMalloc(size);
    }
}

void* dbQueryElement::operator new(size_t size EXTRA_DEBUG_NEW_PARAMS) {
    return dbQueryElementAllocator::instance.allocate(size);
}


void  dbQueryElement::operator delete(void* p EXTRA_DEBUG_NEW_PARAMS)
{
    dbFree(p);
}


dbQueryElementAllocator::~dbQueryElementAllocator() 
{
    dbQueryElement *elem, *next;
    for (elem = freeChain; elem != NULL; elem = next) { 
	next = elem->next;
	delete elem;
    }    
} 

dbQueryExpression& dbQueryExpression::operator = (dbComponent const& comp) 
{ 
    first = NULL; 
    last = &first;
    add(dbQueryElement::qExpression, comp.structure);
    if (comp.field != NULL) { 
	add(dbQueryElement::qExpression, ".");
	add(dbQueryElement::qExpression, comp.field);
    }
    operand = false;
    return *this;
}

dbQueryExpression& dbQueryExpression::operator=(dbQueryExpression const& expr)
{ 
    first = new dbQueryElement(dbQueryElement::qExpression, "(");
    first->next = expr.first;
    last = expr.last;
    *last = new dbQueryElement(dbQueryElement::qExpression, ")");
    last = &(*last)->next;
    operand = false;
    return *this;
}
 
dbQuery& dbQuery::add(dbQueryExpression const& expr) 
{ 
    append(dbQueryElement::qExpression, "(");
    *nextElement = expr.first;
    nextElement = expr.last;
    append(dbQueryElement::qExpression, ")");
    operand = false;
    return *this;
}



dbQuery& dbQuery::reset() 
{ 
    dbQueryElementAllocator::instance.deallocate(elements, nextElement);
    elements = NULL;
    nextElement = &elements;
    operand = false;
    mutexLocked = false;
    dbCompiledQuery::destroy();
    return *this;
}

void dbCompiledQuery::destroy()
{
    if (tree != NULL) {
	dbCriticalSection cs(dbExprNode::mutex);
	delete tree;
	for (dbOrderByNode *op = order, *nop; op != NULL; op = nop) {
	    nop = op->next;
	    delete op;
	}
	for (dbFollowByNode *fp = follow, *nfp; fp != NULL; fp = nfp) {
	    nfp = fp->next;
	    delete fp;
	}
	tree = NULL;
    }
    startFrom = StartFromAny;
    follow = NULL;
    order = NULL;
    table = NULL;
}

int dbUserFunction::getParameterType()
{
    static byte argType[] = {  
	tpInteger,
	tpReal,
	tpString,
	tpInteger,
	tpReal,
	tpString,
	tpInteger,
	tpReal,
	tpString,
	tpInteger,
	tpReal,
	tpString,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList,
	tpList
    };
    return argType[type];
}

int dbUserFunction::getNumberOfParameters()
{
    static byte nArgs[] = {  
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3
    };
    return nArgs[type];
}

dbUserFunction* dbUserFunction::list;


void dbUserFunction::bind(char* fname, void* f, funcType ftype) 
{ 
    name = fname;
    dbSymbolTable::add(name, tkn_ident, FASTDB_CLONE_ANY_IDENTIFIER);
    next = list;
    list = this;
    fptr = f;
    type = ftype;
}

dbUserFunctionArgument::dbUserFunctionArgument(dbExprNode*             expr, 
					       dbInheritedAttribute&   iattr, 
					       dbSynthesizedAttribute& sattr, 
					       int                     i)
{
    dbDatabase::execute(expr->func.arg[i], iattr, sattr);
    switch (expr->func.arg[i]->type) {
      case tpInteger:
	u.intValue = sattr.ivalue;
	type = atInteger;
	break;
      case tpReal:
	u.realValue = sattr.fvalue;
	type = atReal;
	break;
      case tpString:
	u.strValue = sattr.array.base;
	type = atString;
	break;
      case tpBoolean:
	u.boolValue = sattr.bvalue;
	type = atBoolean;
	break;
      case tpReference:
	u.oidValue = sattr.oid;
	type = atReference;
	break;
      case tpRawBinary:
	u.rawValue = sattr.raw;
	type = atRawBinary;
	break;
      default:
	assert(false);
    }
}

