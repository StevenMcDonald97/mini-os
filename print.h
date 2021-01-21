// A class for defining a print function to be used by the OS
#ifndef _PRINT_H_
#define _PRINT_H_

#define LINE_SIZE 160

struct ScreenBuffer {
	char* buffer;
	int column;
	int row;
};
// print used by kernel, with variable number of parameters 
int printk(const char *format, ...);
void write_screen(const char *buffer, int size, char color);

#endif