#include "engine/Common.h"

#include <cstdio>

#include <pspdebug.h>
#include <pspdisplay.h>

namespace Engine::detail
{

[[noreturn]] void panic_impl(
    char const message[], char const file[], int const line)
{
	pspDebugScreenInit();

	pspDebugScreenSetBackColor(0x00FF0000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);

	pspDebugScreenPrintf("Panic: %s\n  at %s:%d", message, file, line);
	printf("Panic: %s\n  at %s:%d", message, file, line);

	while (1) {
		sceDisplayWaitVblankStart();
	}
}

void assert_impl(bool const condition,
    char const message[],
    char const file[],
    int const line)
{
	if (condition) {
		return;
	}

	char buf[1024] { 0 };
	snprintf(buf, sizeof(buf), "Assertion failed: %s", message);
	panic_impl(buf, file, line);
}

} // namespace Engine::detail
