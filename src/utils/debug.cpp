#include "debug.h"
int g_DebugDepth{0};

void StartDestruction(std::string_view name) {
	DEBUG("Destroying " << name << "...");
	++g_DebugDepth;
}

void EndDestruction(std::string_view name) {
	--g_DebugDepth;
	DEBUG("Destroyed " << name);
}
