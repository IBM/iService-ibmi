#include "Runnable.h"

/******************************************************
 * Abstract constructor
 * 
 * Parm:
 *    node  -   ptr to node
 *****************************************************/
Runnable::Runnable(INode* node) 
  : _node_p(node), _error(OFF)
{}

/******************************************************
 * Destructor
 *****************************************************/
Runnable::~Runnable()
{}
