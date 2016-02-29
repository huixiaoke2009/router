#include "common.h"
#include <stdlib.h>
#include <stdio.h>

string vFormat(string fmt, va_list args) {
        char *pBuffer = NULL;
        if (vasprintf(&pBuffer, fmt.c_str(), args) == -1) {
                assert(false);
                return "";
        }
        string result = pBuffer;
        free(pBuffer);
        return result;
}


string format(string fmt, ...) {
        string result = "";
        va_list arguments;
        va_start(arguments, fmt);
        result = vFormat(fmt, arguments);
        va_end(arguments);
        return result;
}

string lowerCase(string value) {
    string result = "";
    for (string::size_type i = 0; i < value.length(); i++) {
            result += tolower(value[i]);
    }
    return result;
}

string upperCase(string value) {
    string result = "";
    for (string::size_type i = 0; i < value.length(); i++) {
            result += toupper(value[i]);
    }
    return result;
}

bool isNumeric(string value) {
    return value == format("%d", atoi(STR(value)));
}

void replace(string &target, string search, string replacement) {
    if (search == replacement)
        return;
    if (search == "")
        return;
    string::size_type i = string::npos;
    string::size_type lastPos = 0;
    while ((i = target.find(search, lastPos)) != string::npos) {
        target.replace(i, search.length(), replacement);
        lastPos = i + replacement.length();
    }
}

