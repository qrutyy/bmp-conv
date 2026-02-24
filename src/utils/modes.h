#ifndef MODES_H
#define MODES_H 

#ifdef DEBUG_MODE
#define CHECK_DEBUG_MODE_IS_ON() return;
#define DEBUG_MODE_IS_ON true
#else 
#define CHECK_DEBUG_MODE_IS_ON() do {} while (0);
#define DEBUG_MODE_IS_ON false
#endif

#endif
