
#include <stdlib.h>
#include <cmath>
#include "Argument.h"
#include "INode.h"
#include "IAttribute.h"
#include <assert.h>
#include <stdio.h>
#include "Utility.h"
#include "FlightRec.h"
#include "ILE.h"
#include <xxcvt.h>

//======================================================================//
//======================= Argument =====================================//
//======================================================================//

/******************************************************
 * Constructor.
 *  
 * Parms:
 *  isPase        -   serve in Pase or not
 ******************************************************/
Argument::Argument(bool isPase)
  : _io(BOTH), _type(INVALID), _bytes(0), _is_hex(false),
    _attr_p(NULL), _value_pase_p(NULL), _value_p(NULL),
    _value_org_p(NULL), _ds_entries(), _is_pase(isPase),
    _sig(0), _by_value(false)
{}

/******************************************************
 * Destructor.
 ******************************************************/
Argument::~Argument()
{
  if( _is_pase ) // Pase
  {
    // free pase memory
    Qp2free(_value_org_p);
  }
  else // ILE
  {
    delete[] _value_org_p;
  }

  // free memory for ds entries if this is DS
  std::list<Argument*>::iterator it = _ds_entries.begin();
  for ( ; it != _ds_entries.end(); ++it )
  {
    delete *it;
  }
}

/******************************************************
 * Get pase pointer for value
 ******************************************************/
QP2_ptr64_t& Argument::valuePasePtr()
{
  return _value_pase_p;
}

/******************************************************
 * Get ILE-use pointer to pase memory
 ******************************************************/
char*& Argument::valueIlePtr()
{
  return _value_p;
}

/******************************************************
 * Get signature for _ILECALL
 ******************************************************/
ILECALL_Arg_t Argument::signature()
{
  return _sig;
}

/******************************************************
 * Get size in bytes for the argument
 ******************************************************/
int Argument::size()
{
  if ( DS != _type )
  {
    assert( _bytes > 0 );
  }
  return _bytes;
}

/******************************************************
 * Set size in bytes for the argument
 ******************************************************/
void Argument::size(int size)
{
  _bytes = size;
}

/******************************************************
 * Enable/disable hexadecimal format
 ******************************************************/
void Argument::hex(bool on)
{
  _is_hex = on;
}

/******************************************************
 * Set up if argument is used in Pase.
 ******************************************************/
void Argument::pase(bool flag)
{
  _is_pase = flag;
  std::list<Argument*>::iterator it = _ds_entries.begin();
  for (; it != _ds_entries.end(); ++it )
  {
    (*it)->pase(_is_pase);
  }
}

/******************************************************
 * Initialize Argument.
 ******************************************************/
bool Argument::init(INode* node, FlightRec& fr)
{
  bool ret = true;
  IAttribute* attr = NULL;
  std::string v;

  //--------------- "type" ----------------------------------
  attr = node->getAttribute("TYPE");
  if ( NULL == attr )
  {
    fr.code(ERR_ARGUMENT_INIT);
    fr.addMessage("MISSING ATTRIBUTE TYPE.");
    ret = false;
    return ret;
  }
  v = attr->valueString();
  cvt2Upper(v);
  trim(v);
  if ( "DS" == v )  // deal with DS in VALUE later
  {
    type(DS);
  }
  else  // normal data types
  {
    if ( ! type(v, fr) ) 
    {
      ret = false;
      return ret;
    }
  }

  if ( type() != DS)
  {
    //--------------- "by" -----------------------------------
    attr = node->getAttribute("BY");
    if ( NULL != attr )
    {
      v = attr->valueString();
      cvt2Upper(v);
      trim(v);
      if ( "REF" == v )  
      {
        byValue(false);
      }
      else if ( "VAL" == v )
      {
        byValue(true);
      }
      else
      {
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(v.c_str(), "UNKONWN VALUE FOR ATTRIBUTE BY.");
        ret = false;
        return ret;
      }
    }

    //--------------- "hex" -----------------------------------
    attr = node->getAttribute("HEX");
    if ( NULL != attr && type() == CHAR )
    {
      v = attr->valueString();
      cvt2Upper(v);
      trim(v);
      if ( "ON" == v )  
      {
        hex(true);
      }
      else if ( "OFF" == v )
      {
        hex(false);
      }
      else
      {
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(v.c_str(), "UNKONWN VALUE FOR ATTRIBUTE HEX.");
        ret = false;
        return ret;
      }
    }
  }

  //--------------- "io" ----------------------------------
  attr = node->getAttribute("IO");
  if ( NULL != attr )
  {
    v = attr->valueString();
    cvt2Upper(v);
    trim(v);
    if ( "IN" == v )
    { 
      output(IN);
    }
    else if ( "OUT" == v  )
    {
      output(OUT);
    }
    else if ( "BOTH" == v )
    {
      output(BOTH);
    }
    else
    {
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage(v.c_str(), "UNKNOWN VALUE FOR TYPE.");
      ret = false;
      return ret;
    }
  }

  //----------------------------------------------------------
  //--------------- "value" ----------------------------------
  //----------------------------------------------------------
  attr = node->getAttribute("VALUE");
  if ( NULL == attr )
  {
    fr.code(ERR_ARGUMENT_INIT);
    fr.addMessage("MISSING ATTRIBUTE VALUE.");
    ret = false;
    return ret;
  }

  if ( type() != DS )
  {
    ////------------ non-ds type value ------------------------
    attribute(attr);
  }
  else  
  {
    ////------------- entries for "DS" ------------------------
    int dsLen = 0;
    Argument* dsArg = this;
    INode* dsNode = attr->nodeValue(); // Node for DS sub-entries
    if ( NULL == dsNode || 0 == dsNode->nodesNum() )
    {
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage("DS SHOULD NOT BE EMPTY.");
      ret = false;
      return ret;;
    }

    std::list<INode*> dsEntries = dsNode->childNodes();
    std::list<INode*>::iterator it2 = dsEntries.begin();
    IAttribute* dsEntAttr = NULL;
    std::string type;
    Argument* dsEnt = NULL;
    for ( ; it2 != dsEntries.end() && ERR_OK == fr.code(); ++it2 )
    {
      dsEnt = new Argument(_is_pase);
      if ( NULL == dsEnt )
      {
        abort();
      }

      dsArg->addDsEntry(dsEnt);

      /////--------- Embedded DS and other type of entries must be by value as default ------
      dsEnt->byValue(true);

      /////------------- populate DS "IO" to entries --------------------------
      dsEnt->output(dsArg->output());

      /////------------- inz ds entry ----------------------------------------
      dsEnt->init(*it2, fr);
      if ( ERR_OK != fr.code() )
      {
        ret = false;
        return ret;
      }

      /////------------- compute DS total length ------------------------------
      if ( ! dsEnt->byValue() ) 
      {
        dsLen += sizeof(char*);
      }
      else
      {
        dsLen += dsEnt->size();
      } 
    }

    // set size for DS
    dsArg->size(dsLen);
  }

  return ret;
}
/*****************************************************************************
 * Parse non-DS types.   
 ****************************************************************************/
bool Argument::type(std::string type, FlightRec& fr)
{
  bool ret = true;
  //---------- convert to uppercase and remove all spaces ----------------
  cvt2Upper(type);
  for ( int i = 0; i < type.length(); ++i )
  {
    if ( JC(SPACE) == type[i] )
    {
      type.erase(i, 1);
    }
  }

  //--------- parse type definiton -----------------------------------------
  if ( type.length() > 6 && "CHAR" == type.substr(0, 4) 
        && JC(OPEN_PAREN) == type[4] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // CHAR()
    _type = CHAR;
    _bytes = atoi(type.substr(5, type.length()-2).c_str());
    if ( 0 == _bytes )
    {
      ret = false;
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage(type.c_str(), "INVALID VALUE FOR CHAR().");
    }
  }
  else if ( type.length() > 5 && "INT" == type.substr(0, 3) 
        && JC(OPEN_PAREN) == type[3] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // INT()
    _type = INT;
    _bytes = atoi(type.substr(4, type.length()-2).c_str());
    if ( 2 != _bytes && 4 != _bytes && 8 != _bytes )
    {
      ret = false;
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage(type.c_str(), "INVALID VALUE FOR INT().");
    }
  }
  else if ( type.length() > 6 && "UINT" == type.substr(0, 4) 
        && JC(OPEN_PAREN) == type[4] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // UINT()
    _type = UINT;
    _bytes = atoi(type.substr(5, type.length()-2).c_str());
    if ( 2 != _bytes && 4 != _bytes && 8 != _bytes )
    {
      ret = false;
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage(type.c_str(), "INVALID VALUE FOR UINT().");
    }
  }
  else if ( type.length() > 7 && "FLOAT" == type.substr(0, 5) 
        && JC(OPEN_PAREN) == type[5] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // FLOAT()
    _type = FLOAT;
    _frac_digits = atoi(type.substr(6, type.length()-2).c_str());
    _bytes = sizeof(float);
  }
  else if ( type.length() > 8 && "DOUBLE" == type.substr(0, 6) 
        && JC(OPEN_PAREN) == type[6] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // DOUBLE()
    _type = DOUBLE;
    _frac_digits = atoi(type.substr(7, type.length()-2).c_str());
    _bytes = sizeof(double);
  }
  else if ( type.length() > 9 && "PACKED" == type.substr(0, 6) 
        && JC(OPEN_PAREN) == type[6] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // PACKED(,)
    _type = PACKED;
    std::size_t comma = type.find(JC(COMMA), 7);
    if ( std::string::npos != comma )
    {
      _total_digits = atoi(type.substr(7, comma-7).c_str());
      _frac_digits = atoi(type.substr(comma+1, type.length()-1-comma+1).c_str());
      if ( 0 != _total_digits % 2 )
      {
        _bytes = (_total_digits + 1)/2;
      }
      else
      {
        _bytes = _total_digits/2 + 1;
      }
    }
    else
    {
      ret = false;
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage(type.c_str(), "INVALID VALUE FOR PACKED().");
    }
  }
  else if ( type.length() > 8 && "ZONED" == type.substr(0, 5) 
        && JC(OPEN_PAREN) == type[5] && JC(CLOSE_PAREN) == type[type.length()-1] )
  { // ZONED(,)
    _type = ZONED;
    std::size_t comma = type.find(JC(COMMA), 6);
    if ( std::string::npos != comma )
    {
      _total_digits = atoi(type.substr(6, comma-6).c_str());
      _frac_digits = atoi(type.substr(comma+1, type.length()-1-comma+1).c_str());
      _bytes = _total_digits; 
    }
    else
    {
      ret = false;
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage(type.c_str(), "INVALID VALUE FOR ZONED().");
    }
  }
  else
  {
    ret = false;
    fr.code(ERR_ARGUMENT_INIT);
    fr.addMessage(type.c_str(), "UNKNOWN PARM TYPE.");
  }


  return ret;
}

/******************************************************
 * Get the type of the argument
 ******************************************************/
Arg_Type Argument::type()
{
  return _type;
}

/******************************************************
 * Get the type of the argument
 ******************************************************/
void Argument::type(Arg_Type type)
{
  _type = type;
}

/******************************************************
 * Get the setting of passing by value
 ******************************************************/
bool Argument::byValue()
{
  return _by_value;
}

/******************************************************
 * Set passing by value
 ******************************************************/
void Argument::byValue(bool byVal)
{
  _by_value = byVal;
}

/******************************************************
 * Get output setting of the argument
 ******************************************************/
IO_Type Argument::output()
{
  return _io;
}

/******************************************************
 * Set output setting of the argument
 ******************************************************/
void Argument::output(IO_Type io )
{
  _io = io;
}

/******************************************************
 * Bind attribute to argument
 ******************************************************/
void Argument::attribute(IAttribute* attr)
{
  _attr_p = attr;

  // set type of attribute "value"
  assert( INVALID != _type );
  if ( CHAR == _type )
  {
    _attr_p->type(STRING);
  }
  else
  {
    _attr_p->type(NUMBER);
  }
}

/*****************************************************
 * Add DS entries.
 * Parm:
 *    subArg    - sub-entries of parm
 ****************************************************/
void Argument::addDsEntry(Argument* subArg)
{
  _ds_entries.push_back(subArg);
}

/******************************************************
 * Get DS entries
 ******************************************************/
std::list<Argument*>& Argument::dsEntries()
{
  return _ds_entries;
}

/****************************************************************
 * Allocate memory and copy data in for progam call arguments.
 * Parms:
 *    fr      - error information
 *    srvpgm  - indicates if signature needs to be initialized
 * Return:
 *    true  - successful
 *    false - failed
 ***************************************************************/
bool Argument::copyIn(FlightRec& fr, bool srvpgm)
{
  assert( INVALID != _type );

  bool ret = true;

  if ( DS == _type ) // "DS"
  {
    //----------------- allocate memory for DS ------------------------
    if ( _is_pase ) // pase call
    {
      _value_org_p = Pase::allocAlignedPase(&_value_p, &_value_pase_p, _bytes, fr);
    } // ile call
    else
    {
      _value_org_p = new char[_bytes];
      if ( NULL == _value_org_p )
      {
        abort();
      }
      else
      {
        _value_p = _value_org_p;
      }
    }

    //----------------- inz ds signature -----------------------------------
    if ( srvpgm )
    { // Note: this only used for the top level DS as a parameter for PGM/SRVPGM
      _sig = ARG_SPCPTR;
    }
    
    bool good = true;
  
    //----------------- inz ds entries and assemble DS data from entries ---
    char* where = _value_p;
    std::list<Argument*>::iterator it = _ds_entries.begin();
    
    for (int ind = 0; it != _ds_entries.end() && good ; ++it, ++ind )
    {
      good = (*it)->copyIn(fr, false); // ds entries don't need a signature
      if ( good )
      {
        if ( (*it)->byValue() ) // "by":"val"
        {
          if ( OUT != _io )
          {
            memcpy(where, (*it)->valueIlePtr(), (*it)->size());
          }
          where += (*it)->size();
        }
        else // "by":"ref"
        {
          memcpy(where, (char*)(&((*it)->valueIlePtr())), sizeof(char*));
          where += sizeof(char*);
        }
      }
      else
      {
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(ind+1, "FAILED TO PROCESS NTH ENTRY IN DS.");
        ret = false;
        return ret;
      }
    }


    ret = good;
  }
  else  // Normal data types
  {
    if ( _is_pase ) // pase call
    {
      _value_org_p = Pase::allocAlignedPase(&_value_p, &_value_pase_p, _bytes, fr);
    } // ile call
    else
    {
      _value_org_p = new char[_bytes];
      if ( NULL == _value_org_p )
      {
        abort();
      }
      else
      {
        _value_p = _value_org_p;
      }
    }

    if ( NULL != _value_org_p )
    {
      const char* s = NULL;
      char* e = NULL;

      if ( CHAR == _type )
      {
        if ( srvpgm )
        {
          _sig = _by_value ? _bytes : ARG_SPCPTR;
        }

        if ( OUT != _io )
        {
          memset(_value_p, JC(SPACE), _bytes);
          char* pIn = const_cast<char*>(_attr_p->valueString().c_str());
          if ( ! _is_hex ) //// normal chars
          {
            size_t iLen = _attr_p->valueString().length();
            memcpy(_value_p, pIn, iLen); 
          }
          else //// char* to hexidecimal
          {
            size_t iLen = _attr_p->valueString().length();
            if ( iLen % 2 == 0 && iLen/2 < _bytes )
            {
              int ind = 0; 
              char* out = _value_p;
              while ( ind < iLen/2 )
              {
                char s[3] = {'\0'};
                memcpy(s, pIn, 2);
                char* e = NULL;
                char c = (char) strtol(s, &e, 16);
                if ( EINVAL != errno && ERANGE != errno )
                {
                  *out = c;
                }
                else
                {
                  break;
                }
                ++ind;
                pIn += 2;
                ++out;
              }

              if ( ind != iLen/2 )
              {
                fr.code(ERR_ARGUMENT_INIT);
                fr.addMessage(_value_p, "INVALID HEXADECIMAL CHARS.");
                ret = false;
              }
            }
          }
        }
      }
      else if ( INT ==  _type )
      {
        s = _attr_p->valueString().c_str();
        e = NULL;
        long long int lli = strtoll(s, &e, 10);
        if ( EINVAL != errno && ERANGE != errno )
        {
          if ( 2 == _bytes ) 
          {
            if ( OUT != _io )
            {            
              short int si = (short int) lli;
              memcpy(_value_p, &si, sizeof(short int));
            }

            if ( srvpgm )
            {
              _sig = _by_value ? ARG_INT16 : ARG_SPCPTR;
            }
          }
          else if ( 4 == _bytes )
          {
            if ( OUT != _io )
            { 
              int i = (int) lli;
              memcpy(_value_p, &i, sizeof(int));
            }

            if ( srvpgm )
            {
              _sig = _by_value ? ARG_INT32 : ARG_SPCPTR;
            }
          }
          else if ( 8 == _bytes )
          {
            if ( OUT != _io )
            { 
              memcpy(_value_p, &lli, sizeof(long long int));
            }

            if ( srvpgm )
            {
              _sig = _by_value ? ARG_INT64 : ARG_SPCPTR;
            }
          }
          else 
          {
            fr.code(ERR_ARGUMENT_INIT);
            fr.addMessage(_value_p, "INVALID BYTES NUMBER FOR INT().");
            ret = false;
          }        
        }
        else
        {
          fr.code(ERR_ARGUMENT_INIT);
          fr.addMessage(_value_p, "INVALID BYTES NUMBER FOR INT().");
          ret = false;              
        } 
      }
      else if ( UINT == _type )
      {
        if ( OUT != _io )
        { 
          s = _attr_p->valueString().c_str();
          e = NULL;
          unsigned long long int ulli = strtoull(s, &e, 10);
          if ( EINVAL != errno && ERANGE != errno )
          {
            if ( 2 == _bytes ) 
            {
              unsigned short int si = (unsigned short int) ulli;
              memcpy(_value_p, &si, sizeof(unsigned short int));
              if ( srvpgm )
              {
                _sig = _by_value ? ARG_UINT16 : ARG_SPCPTR;
              }
            }
            else if ( 4 == _bytes ) 
            {
              unsigned int i = (unsigned int) ulli;
              memcpy(_value_p, &i, sizeof(unsigned int));
              if ( srvpgm )
              {
                _sig = _by_value ? ARG_UINT32 : ARG_SPCPTR;
              }
            }
            else if ( 8 == _bytes ) 
            {
              memcpy(_value_p, &ulli, sizeof(unsigned long long int));
              if ( srvpgm )
              {
                _sig = _by_value ? ARG_UINT64 : ARG_SPCPTR;
              }
            }
            else
            {
              fr.code(ERR_ARGUMENT_INIT);
              fr.addMessage(_value_p, "INVALID BYTES NUMBER FOR UINT().");
              ret = false;              
            } 
          }
          else
          {
            fr.code(ERR_ARGUMENT_INIT);
            fr.addMessage(_value_p, "INVALID VALUE FOR UINT().");
            ret = false;              
          } 

          if ( srvpgm )
          {
            if ( 2 == _bytes ) 
            {
              _sig = _by_value ? ARG_UINT16 : ARG_SPCPTR;
            }
            else if ( 4 == _bytes ) 
            {
              _sig = _by_value ? ARG_UINT32 : ARG_SPCPTR;
            }
            else if ( 8 == _bytes ) 
            {
              _sig = _by_value ? ARG_UINT64 : ARG_SPCPTR;
            }
            else
            {} 
          }
          
        }
      }
      else if ( FLOAT == _type )
      {
        if ( srvpgm )
        {
          _sig = _by_value ? ARG_FLOAT32 : ARG_SPCPTR;
        }

        if ( OUT != _io )
        {   
          s = _attr_p->valueString().c_str();
          e = NULL;
          float f = strtof(s, &e);
          if ( HUGE_VALF != f && -HUGE_VALF != f && ERANGE != errno )
          {
            memcpy(_value_p, &f, sizeof(float));
          }
          else
          {
            fr.code(ERR_ARGUMENT_INIT);
            fr.addMessage(_value_p, "INVALID VALUE FOR FLOAT().");
            ret = false;    
          }
        }
      }
      else if ( DOUBLE == _type )
      {
        if ( srvpgm )
        {
          _sig = _by_value ? ARG_FLOAT64 : ARG_SPCPTR;
        }

        if ( OUT != _io )
        { 
          s = _attr_p->valueString().c_str();
          e = NULL;
          double d = strtod(s, &e);
          if ( HUGE_VALF != d && -HUGE_VALF != d && ERANGE != errno )
          {
            memcpy(_value_p, &d, sizeof(double));
          }
          else
          {
            fr.code(ERR_ARGUMENT_INIT);
            fr.addMessage(_value_p, "INVALID VALUE FOR DOUBLE().");
            ret = false;    
          }
        }
      }
      else if ( PACKED == _type )
      {
        if ( srvpgm )
        {
          _sig = _by_value ? _bytes : ARG_SPCPTR;
        }

        if ( OUT != _io )
        { 
          s = _attr_p->valueString().c_str();
          e = NULL;
          double d = strtod(s, &e);
          if ( HUGE_VAL != d && -HUGE_VAL != d && ERANGE != errno )
          {
            unsigned char p[256] = {'\0'};
            #pragma exception_handler ( qxxdtop_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
            QXXDTOP(p, _total_digits, _frac_digits, d);
            #pragma disable_handler

            memcpy(_value_p, p, _bytes);
          }
          else
          {
            qxxdtop_ex:
            fr.code(ERR_ARGUMENT_INIT);
            fr.addMessage(_value_p, "INVALID VALUE FOR PACKED().");
            ret = false;    
          }
        }
      }
      else if ( ZONED == _type )
      {
        if ( srvpgm )
        {
          _sig = _by_value ? _bytes : ARG_SPCPTR;
        }

        if ( OUT != _io )
        { 
          s = _attr_p->valueString().c_str();
          e = NULL;
          double d = strtod(s, &e);
          if ( HUGE_VAL != d && -HUGE_VAL != d && ERANGE != errno )
          {
            unsigned char p[256] = {'\0'};
            #pragma exception_handler ( qxxdtoz_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
            QXXDTOZ(p, _total_digits, _frac_digits, d);
            #pragma disable_handler

            memcpy(_value_p, p, _bytes);
            ret = true;
          }
          else
          {
            qxxdtoz_ex:
            fr.code(ERR_ARGUMENT_INIT);
            fr.addMessage(_value_p, "INVALID VALUE FOR ZONED().");
            ret = false;    
          }
        }
      }
      else
      {
        assert(false);
      }
    }
  }

  return ret;
}

/******************************************************
 * Copy value from argument back to attribute
 * 
 * Parm:
 *    fr  - error information
 ******************************************************/
void Argument::copyBack(FlightRec& fr)
{
  if ( _io != IN )
  {
    if ( DS != _type ) // normal
    {
      char s[256] = {'\0'};
      if ( PACKED == _type )
      {
        #pragma exception_handler ( qxxptod_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
        double d = QXXPTOD(reinterpret_cast<unsigned char*>(_value_p), _total_digits, _frac_digits);
        #pragma disable_handler
        sprintf(s, "%-*.*lf", _total_digits, _frac_digits, d);
        std::string str(s);
        _attr_p->setValue(str);
        return;
        qxxptod_ex:
          fr.code(ERR_PGM_RUN);
          fr.addMessage("FAILED TO COPY PACKED DECIMAL BACK FOR ARGUMENT.");
      }
      else if ( ZONED == _type )
      {
        #pragma exception_handler ( qxxztod_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
        double d = QXXZTOD(reinterpret_cast<unsigned char*>(_value_p), _total_digits, _frac_digits);
        #pragma disable_handler
        sprintf(s, "%-*.*lf", _total_digits, _frac_digits, d);
        std::string str(s);
        _attr_p->setValue(str);
        return;
        qxxztod_ex:
        fr.code(ERR_PGM_RUN);
        fr.addMessage("FAILED TO COPY ZONED DECIMAL BACK FOR ARGUMENT.");
      }
      else if ( FLOAT == _type )
      {
        sprintf(s, "%.*f", _frac_digits, *((float*) _value_p));
        std::string str(s);
        _attr_p->setValue(str);
      }
      else if ( DOUBLE == _type ) 
      {
        sprintf(s, "%.*lf", _frac_digits, *((double*) _value_p));
        std::string str(s);
        _attr_p->setValue(str);
      }
      else if ( INT == _type )
      {
        switch ( _bytes )
        {
        case 2:
          sprintf(s, "%d", *((short int*) _value_p));
          break;
        case 4:
          sprintf(s, "%d", *((int*) _value_p));
          break;
        case 8:
          sprintf(s, "%lld", *((long long int*) _value_p));
          break;
        default:
          assert(false); 
          break;
        }
        std::string str(s);
        _attr_p->setValue(str);
      }
      else if ( UINT == _type )
      {
        switch ( _bytes )
        {
        case 2:
          sprintf(s, "%u", *((unsigned short int*) _value_p));
          break;
        case 4:
          sprintf(s, "%u", *((unsigned int*) _value_p));
          break;
        case 8:
          sprintf(s, "%llu", *((unsigned long long int*) _value_p));
          break;
        default:
          assert(false); 
          break;
        }
        std::string str(s);
        _attr_p->setValue(str);
      }
      else if ( CHAR == _type )
      {
        if ( ! _is_hex ) //// normal chars
        {
          std::string v(_value_p, _bytes);
          /*************************************************************
           * Better keep exactly what the called pgm returned to leave
           * choices to caller.
           *************************************************************/ 
          // trim(v);
          _attr_p->setValue(v);
        }
        else  //// char* to hexadecimal
        {
          std::string v;
          char* out = _value_p;
          trimTrailing(out, _bytes, JC(SPACE));
          for ( int ind = 0; ind <= _bytes && *out != '\0'; ++ind, ++out )
          {
            char s[3] = {'\0'};
            sprintf(s, "%02X", *out);
            v += s;
          }
          _attr_p->setValue(v);
        }
      }
      else
      { // unknown type
        assert(false);
      }
    }
    else  // DS
    {
      //---------- copy data back to entries from ds memory ---------
      char* where = _value_p;
      std::list<Argument*>::iterator it = _ds_entries.begin();
      for ( ; it != _ds_entries.end() && ERR_OK == fr.code(); ++it )
      {
        Argument* arg = *it;
        if ( arg->byValue() ) // "by":"val"
        {
          memcpy(arg->valueIlePtr(), where, arg->size());
          where += arg->size();
        }
        else // "by":"ref" - value has been changed in pgm/srvpgm called
        {
          where += sizeof(char*);
        }

        arg->copyBack(fr);
      }
    }
  }
}

//=======================================================================//
//======================= ArgumentS =====================================//
//=======================================================================//

/******************************************************
 * Constructor
 ******************************************************/
Arguments::Arguments()
: _is_pase(false), _args(), _argv_pase_p(NULL), _argv_p(NULL),
  _argv_org_p(NULL), _node_p(NULL), _sigs_p(NULL), 
  _sigs_org_p(NULL), _sigs_pase_p(NULL), _all_by_ref(true),
  _init_done(true)
{}

/******************************************************
 * Destructor
 ******************************************************/
Arguments::~Arguments()
{
  /********************************************************
   * In Pase call,
   *  _argv_pase_p and _argv_p are aligned ptrs reside in
   *  _argv_org_p. So leave them alone.
   * In ILE call,
   *  only free memory for _argv_p. _argv_org_p and 
   *  _argv_pase_p are not used.
   *******************************************************/
  if (_is_pase)
  {
    // free pase memory for argv
    Qp2free(_argv_org_p); 
    // free pase memory for each argument
    std::list<Argument*>::iterator it = _args.begin();
    for ( ; it != _args.end(); ++it )
    {
      delete *it;
    }
    // free signatures
    if ( NULL != _sigs_p )
    {
      Qp2free(_sigs_p); 
    }
  }
  else
  {
    delete[] _argv_p;
  }
  
}

bool Arguments::allByReference() 
{
  assert( true == _init_done );
  return _all_by_ref;
}

/************************************************************
 * Parse PARM node. 
 * Parms:
 *    parm  - ptr to PARM to parse
 *    fr    - error information
 ************************************************************/
void Arguments::init(INode* parm, FlightRec& fr)
{
  assert( NULL != parm );
  _node_p = parm;

  if ( _node_p->nodesNum() > 0 ) // with parms
  {
    std::list<INode*> nodes = _node_p->childNodes();
    std::list<INode*>::iterator it = nodes.begin();
    Argument* arg = NULL;

    for ( ; it != nodes.end(); ++it ) // process each parm
    {
      arg = new Argument(_is_pase);
      if ( NULL == arg )
      {
        abort();
      }
      else
      {
        _args.push_back(arg);
      }

      if ( ! arg->init(*it, fr) )
      {
        return;
      }

      if ( arg->byValue() )
      {
        _all_by_ref = false;
      }
    }
  }
  
  _init_done = true;
}

/******************************************************
 * Get arguments number
 ******************************************************/
int  Arguments::number()
{
  return _args.size();
}

/******************************************************
 * Set being used in Pase also for all sub-entries.
 * 
 * Parm:
 *    isPase  - used in Pase or not
 ******************************************************/
void Arguments::pase(bool isPase)
{
  _is_pase = isPase;
  std::list<Argument*>::iterator it = _args.begin();
  for (; it != _args.end(); ++it )
  {
    (*it)->pase(_is_pase);
  }
}

/******************************************************
 * Get setting about being used in Pase
 ******************************************************/
bool Arguments::isPase()
{
  return _is_pase;
}

/******************************************************
 * ILE-use pointer to pase memory of argv
 ******************************************************/
char* Arguments::ilePtr()
{
  return _argv_p;
}

/******************************************************
 * Get pase pointer of argv for Pase call
 ******************************************************/
QP2_ptr64_t* Arguments::paseArgv()
{
  return &_argv_pase_p;
}

/******************************************************
 * Get argv pointer for ILE call
 ******************************************************/
char** Arguments::ileArgv()
{
  return &_argv_p;
}

/******************************************************
 * Get signature for Pase call
 ******************************************************/
QP2_ptr64_t* Arguments::signatures()
{
  return &_sigs_pase_p;
}

/*************************************************************
 * Create argv and signatures(srvpgm only)
 * Parms:
 *    fr      - (output) error information
 *    srvpgm  - true for srvpgm
 ************************************************************/
void Arguments::create(FlightRec& fr, bool srvpgm)
{
  if ( 0 == number()  ) // no arguments 
  {
    return;
  }

  //------------ compose argv from arguments ---------------------------
  if ( _is_pase ) // for pase call
  {
    ////---------- allocate pase memory for argv -----------------------
    int bytes = 0; 
    if ( srvpgm ) // srvpgm call for _ILECALL()
    {
      //////-------- caculate bytes considering "by":"Val" ----------------------
      std::list<Argument*>::iterator it = _args.begin();
      for (int i = 0 ; it != _args.end(); ++it, ++i )
      {
        Argument* arg = *it;
        if ( DS == arg->type() )
        { //// Let DS always use "by":"ref" !!
          // std::list<Argument*> dsEnts = arg->dsEntries();
          // std::list<Argument*>::iterator it2 = dsEnts->begin();
          // for (int j = 0 ; it2 != _args.end(); ++it2, ++j )
          // {
          //   Argument* dsEnt = *it2;
          //   if ( dsEnt->byValue() )
          //   {
          //     bytes += dsEnt->size();
          //   }
          //   else
          //   {
          //     bytes += sizeof(ILEpointer);
          //   }
          // }
          bytes += 16; // make enough space for alignment
        }
        else 
        {
          if ( arg->byValue() )
          {
            bytes += arg->size() + 16; // make sure enough space for alignment
          }
          else
          {
            bytes += 16;
          }
        }
      }

      // create pase memory for signatures
      _sigs_org_p = Pase::allocAlignedPase(&_sigs_p, &_sigs_pase_p,
                                          (number()+1)*sizeof(ILECALL_Arg_t), fr);
      if ( NULL == _sigs_org_p )
      {
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR SIGNATURES.");
        return;
      }
      else
      {
        // make sure last one is set to ARG_END(0)
        memset(_sigs_org_p, 0, (number()+1)*sizeof(ILECALL_Arg_t));
      }
    }
    else // pgm call for _PGMCALL()
    {
      bytes = (number()+1) * sizeof(QP2_ptr64_t);
    }
    
    _argv_org_p = Pase::allocAlignedPase(&_argv_p, &_argv_pase_p, bytes, fr);
    if ( NULL == _argv_org_p )
    {
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage("FAILED TO ALLOCATE PASE MEMORY FOR ARGV.");
      return;
    }
    memset(_argv_org_p, 0, bytes); 

    ////---------- assemble argv and signatures(srvpgm only) -------------
    char* pBase = _argv_p;
    char* pWork = NULL;
    if ( srvpgm )
    {
      pBase = _argv_p + sizeof(ILEarglist_base);
      pWork = pBase;
    }
    std::list<Argument*>::iterator it = _args.begin();
    for (int i = 0 ; it != _args.end() && ERR_OK == fr.code(); ++it, ++i )
    {
      Argument* arg = *it;
      if ( ! arg->copyIn(fr, srvpgm) )
      {
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(i+1, "FAILED TO CREATE NTH ELEMENT FOR ARGV");
      }
      else
      {
        if ( srvpgm )
        {
          // init arg list
          if ( arg->byValue() )
          {
            if ( arg->signature() > 0 ) // aggregates
            {
              // No need to pad for char[] OR packed OR zoned
            }
            else
            {
              pWork = ILE::alignPadding(pWork, pWork-pBase, arg->size());
            }
            memcpy( pWork, arg->valueIlePtr(), arg->size());
            pWork += arg->size();
          }
          else
          {
            pWork = ILE::alignPadding(pWork, pWork-pBase, sizeof(ILEpointer));
            char*& p = arg->valueIlePtr();
            memcpy( pWork, (char*) &p,  sizeof(ILEpointer));
            pWork += sizeof(ILEpointer);
          }
          
          // init sig list
          ILECALL_Arg_t sig = arg->signature();
          memcpy(_sigs_p + i*sizeof(ILECALL_Arg_t), &sig,  sizeof(ILECALL_Arg_t));
        }
        else
        {
          QP2_ptr64_t paseP = arg->valuePasePtr();
          memcpy( pBase + i*sizeof(QP2_ptr64_t), (char*) &paseP,  sizeof(QP2_ptr64_t));
        }
      }
    }
  }
  else  // for ile call
  {
    // allocate 1 more as NULL for pgmcall
    int bytes = (number() + 1) * sizeof(char*); 

    _argv_org_p = _argv_p = new char[bytes];
    if ( NULL == _argv_p )
    {
      fr.code(ERR_ARGUMENT_INIT);
      fr.addMessage("FAILED TO ALLOCATE MEMORY FOR ARGV.");
      return;
    }
    else
    {
      memset(_argv_p, 0x00, bytes);
    }
    std::list<Argument*>::iterator it = _args.begin();
    for (int i = 0 ; it != _args.end() && ERR_OK == fr.code(); ++it, ++i )
    {
      Argument* arg = *it;
      if ( ! arg->copyIn(fr, srvpgm) )
      {
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(i+1, "FAILED TO CREATE NTH ELEMENT FOR ARGV");
      }
      else
      {
        char*& p = arg->valueIlePtr();
        memcpy( _argv_p + i * sizeof(char*), (char*)&p, sizeof(char*));
      }
    }
  }
  return;
}

/******************************************************
 * Copy data from arguments back to attributes
 ******************************************************/
void Arguments::copyBack(FlightRec& fr)
{
  std::list<Argument*>::iterator it = _args.begin();
  for ( ; it != _args.end(); ++it )
  {
    (*it)->copyBack(fr);
  }
}

//=======================================================================//
//======================= Return ========================================//
//=======================================================================//

/******************************************************
 * Constructor
 ******************************************************/
Return::Return()
  : _type(RESULT_VOID), _bytes(0), _attr_p(NULL)
{}

/******************************************************
 * Destructor
 ******************************************************/
Return::~Return()
{}

/******************************************************
 * Get return value type
 ******************************************************/
Result_Type_t Return::type()
{
  return _type;
}

/******************************************************
 * Bind return attribute to return value
 * Parm:
 *  attr   - node attribute of RETURN
 ******************************************************/
void Return::attribute(IAttribute* attr)
{
  _attr_p = attr;
}

/******************************************************
 * Call via ILE only if return is VOID.
 *****************************************************/
bool Return::isVoid()
{
  return RESULT_VOID == _type ;
}

/******************************************************
 * Initialize return instance
 * 
 * Parm:
 *    node  - RETURN node
 *    fr    - error information
 ******************************************************/
bool Return::init(INode* node, FlightRec& fr)
{
  bool ret = true;
  //-------------------------- "value" -----------------------------------------
  _attr_p = node->getAttribute("VALUE");
  if ( NULL == _attr_p )
  {
    ret = false;
    fr.code(ERR_RETURN_INIT);
    fr.addMessage("EXPECTING ATTRIBUTE VALUE FOR RETURN.");
  }

  //--------------- "type" -----------------------------------
  IAttribute* attr = NULL;
  std::string type;
  attr = node->getAttribute("TYPE");
  if ( NULL != attr )
  {
    type = attr->valueString();
    cvt2Upper(type);
    trim(type);

    if ( type.length() > 5 && "INT" == type.substr(0, 3) 
          && JC(OPEN_PAREN) == type[3] && JC(CLOSE_PAREN) == type[type.length()-1] )
    { // INT()
      _attr_p->type(NUMBER);
      _bytes = atoi(type.substr(4, type.length()-2).c_str());
      if ( 2 == _bytes )
      {
        _type = RESULT_INT16;
      }
      else if ( 4 == _bytes )
      {
        _type = RESULT_INT32;
      }
      else if ( 8 == _bytes )
      {
        _type = RESULT_INT64;
      }
      else
      {
        ret = false;
        fr.code(ERR_RETURN_INIT);
        fr.addMessage(type.c_str(), "INVALID VALUE FOR INT().");
      }
    }
    else if ( type.length() > 6 && "UINT" == type.substr(0, 4) 
          && JC(OPEN_PAREN) == type[4] && JC(CLOSE_PAREN) == type[type.length()-1] )
    { // UINT()
      _attr_p->type(NUMBER);
      _bytes = atoi(type.substr(5, type.length()-2).c_str());
      if ( 2 == _bytes )
      {
        _type = RESULT_UINT16;
      }
      else if ( 4 == _bytes )
      {
        _type = RESULT_UINT32;
      }
      else if ( 8 == _bytes )
      {
        _type = RESULT_UINT64;
      }
      else
      {
        ret = false;
        fr.code(ERR_RETURN_INIT);
        fr.addMessage(type.c_str(), "INVALID VALUE FOR UINT().");
      }
    }
    else if ( type.length() > 8 && "FLOAT" == type.substr(0, 5) 
          && JC(OPEN_PAREN) == type[5] && JC(CLOSE_PAREN) == type[type.length()-1] )
    { // FLOAT(,)
      _attr_p->type(NUMBER);
      _type = RESULT_FLOAT64;
      std::size_t comma = type.find(JC(COMMA), 6);
      if ( std::string::npos != comma )
      {
        _total_digits = atoi(type.substr(6, comma-6).c_str());
        _frac_digits = atoi(type.substr(comma+1, type.length()-1-comma+1).c_str());
        _bytes = sizeof(float);
      }
      else
      {
        ret = false;
        fr.code(ERR_RETURN_INIT);
        fr.addMessage(type.c_str(), "INVALID VALUE FOR FLOAT().");
      }
    }
    else if ( type.length() > 8 && "DOUBLE" == type.substr(0, 6) 
          && JC(OPEN_PAREN) == type[6] && JC(CLOSE_PAREN) == type[type.length()-1] )
    { // DOUBLE(,)
      _attr_p->type(NUMBER);
      _type = RESULT_FLOAT64;
      std::size_t comma = type.find(JC(COMMA), 7);
      if ( std::string::npos != comma )
      {
        _total_digits = atoi(type.substr(7, comma-7).c_str());
        _frac_digits = atoi(type.substr(comma+1, type.length()-1-comma+1).c_str());
        _bytes = sizeof(double);
      }
      else
      {
        ret = false;
        fr.code(ERR_RETURN_INIT);
        fr.addMessage(type.c_str(), "INVALID VALUE FOR DOUBLE().");
      }
    }
    else if ( type.length() > 9 && "PACKED" == type.substr(0, 6) 
          && JC(OPEN_PAREN) == type[6] && JC(CLOSE_PAREN) == type[type.length()-1] )
    { // PACKED(,)
      _type = RESULT_PACKED15;
      std::size_t comma = type.find(JC(COMMA), 7);
      if ( std::string::npos != comma )
      {
        _total_digits = atoi(type.substr(7, comma-7).c_str());
        _frac_digits = atoi(type.substr(comma+1, type.length()-1-comma+1).c_str());
        if ( 0 != _total_digits % 2)
        {
          _bytes = (_total_digits + 1)/2;
        }
        else
        {
          _bytes = _total_digits/2 + 1;
        }
      }
      else
      {
        ret = false;
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(type.c_str(), "INVALID VALUE FOR PACKED().");
      }
    }
    else if ( type.length() > 8 && "ZONED" == type.substr(0, 6) 
          && JC(OPEN_PAREN) == type[6] && JC(CLOSE_PAREN) == type[type.length()-1] )
    { // ZONED(,)
      _type = RESULT_PACKED31;
      std::size_t comma = type.find(JC(COMMA), 6);
      if ( std::string::npos != comma )
      {
        _total_digits = atoi(type.substr(6, comma-6).c_str());
        _frac_digits = atoi(type.substr(comma+1, type.length()-1-comma+1).c_str());
        _bytes = _total_digits; 
      }
      else
      {
        ret = false;
        fr.code(ERR_ARGUMENT_INIT);
        fr.addMessage(type.c_str(), "INVALID VALUE FOR ZONED().");
      }
    }
    else
    {
      ret = false;
      fr.code(ERR_RETURN_INIT);
      fr.addMessage(type.c_str(), "UNKNOWN RETURN TYPE.");
    }
  }
  else // default to INT(4)
  {
    // ret = false;
    // fr.code(ERR_RETURN_INIT);
    // fr.addMessage("EXPECTING ATTRIBUTE TYPE FOR RETURN.");

    _attr_p->type(NUMBER);
    _type = RESULT_INT32;
    _bytes = 4;
  }
  
  return ret;
}

/******************************************************
 * Copy return value back to bound RETURN attribute
 * 
 * Parm:
 *    result  - buffer to return value
 *    fr      - error information
 ******************************************************/
void Return::copyBack(char* result, FlightRec& fr)
{
  if ( NULL == result || RESULT_VOID == _type ) 
  {
    return;
  }

  char s[32] = {'\0'};
  std::string str;
  ILEarglist_base* base = (ILEarglist_base*) result;
  if ( RESULT_INT16 == _type )
  {
    sprintf(s, "%d", base->result.s_int16.r_int16);
  }
  else if ( RESULT_INT32 == _type )
  {
    sprintf(s, "%d", base->result.s_int32.r_int32);
  }
  else if ( RESULT_INT64 == _type )
  {
    sprintf(s, "%lld", base->result.r_int64);
  }
  else if ( RESULT_UINT16 == _type )
  {
    sprintf(s, "%u", base->result.s_uint16.r_uint16);
  }
  else if ( RESULT_UINT32 == _type )
  {
    sprintf(s, "%u", base->result.s_uint32.r_uint32);
  }
  else if ( RESULT_UINT64 == _type )
  {
    sprintf(s, "%llu", base->result.r_uint64);
  }
  else if ( RESULT_FLOAT64 == _type )
  {
    sprintf(s, "%-*.*lf", _total_digits, _frac_digits, base->result.r_float64);
  }
  else if ( RESULT_PACKED15 == _type )
  {
    #pragma exception_handler ( qxxptod_ex, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
    double d = QXXPTOD((unsigned char*)(base->result.r_aggregate), 
                          _total_digits, _frac_digits);
    #pragma disable_handler
    sprintf(s, "%-*.*lf", _total_digits, _frac_digits, d);
    return;
    qxxptod_ex:
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO COPY PACKED DECIMAL BACK FOR RETURN.");
  }
  else if ( RESULT_PACKED31 == _type )
  {
    #pragma exception_handler ( qxxztod_exr, 0, _C1_ALL, _C2_ALL, _CTLA_HANDLE )
    double d = QXXZTOD((unsigned char*)(base->result.r_aggregate), 
                          _total_digits, _frac_digits);
    #pragma disable_handler
    sprintf(s, "%-*.*lf", _total_digits, _frac_digits, d);
    return;
    qxxztod_exr:
      fr.code(ERR_PGM_RUN);
      fr.addMessage("FAILED TO COPY ZONED DECIMAL BACK FOR RETURN.");
  }
  else
  {
    assert(false);
  }
  str += s;
  _attr_p->setValue(str);
}

/****************************************************************
 * Copy pgm return value back to attribute "RETURN"
 * 
 * Parms:
 *    rtnValue  - return value
 ***************************************************************/
void Return::copyBack(int rtnValue)
{
  if ( NULL != _attr_p )
  {
    char s[32] = {'\0'};
    std::string str;
    sprintf(s, "%d", rtnValue);
    str += s;
    _attr_p->setValue(str);
  }
}