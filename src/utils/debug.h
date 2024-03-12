#ifndef PORTAL2RAYTRACED_DEBUG_H
#define PORTAL2RAYTRACED_DEBUG_H

//#ifdef NDEBUG
//#define DEBUG(x)
//#else
#include <iostream>
extern int g_DebugDepth;

#define DEBUG(x) std::cout << "[DEBUG]: " << std::string(g_DebugDepth, '-') << x << std::endl

void StartDestruction(std::string_view name);

void EndDestruction(std::string_view name);

//#endif

#endif//PORTAL2RAYTRACED_DEBUG_H
