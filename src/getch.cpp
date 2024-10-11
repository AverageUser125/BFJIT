#include "main.hpp"
#include "getch.hpp"

#if PLATFORM_LINUX
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <curses.h>
#include <ncurses.h>

char MyGetch(void) {
	return getchar();
}
#elif PLATFORM_WINDOWS
#include <conio.h>

char MyGetch(void) {
	return _getch();
}
#endif