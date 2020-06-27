// ReflectionTool.cpp : Defines the entry point for the console application.
//

#define WIN32_MEAN_AND_LEAN

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "core/container/string_view.h"

void listdir(const char *path, int indent) {
	const char* indent_str = "                           ";
	
	char search[100];
	sprintf_s(search, "%s/*", path);

	_WIN32_FIND_DATAA find_file_data ;
	HANDLE hFind = FindFirstFileA(search, &find_file_data);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("%.*sempty\n", indent, indent_str);
		return;
	}

	do {
		string_view file_name = find_file_data.cFileName;
		bool is_directory = find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		
		char buffer[100];

		if (is_directory && !file_name.starts_with(".")) {
			char search[500];
			sprintf_s(search, "%s/%s", path, file_name.data);

			printf("%.*s%s/\n", indent, indent_str, file_name.data);
			listdir(search, indent + 4);
		}
		else if (file_name.ends_with(".h")) {
			printf("%.*s%s\n", indent, indent_str, file_name.data);
		}
	} while (FindNextFileA(hFind, &find_file_data) != 0);

	FindClose(hFind);
}
