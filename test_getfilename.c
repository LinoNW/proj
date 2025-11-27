#include <stdio.h>
#include <string.h>

char *get_filename(char *pathname)
{
    if (pathname == NULL)
        return NULL;
    
    char *last_slash = strrchr(pathname, '/');
    if (last_slash == NULL)
    {
        return pathname; // No slash found, entire pathname is filename
    }
    
    // Skip the slash and return filename
    char *filename = last_slash + 1;
    // If filename is empty (path ends with '/'), return NULL
    if (*filename == '\0')
        return NULL;
    return filename;
}

int main() {
    printf("get_filename(\"\") = %s\n", get_filename("") ? get_filename("") : "NULL");
    printf("get_filename(\"/\") = %s\n", get_filename("/") ? get_filename("/") : "NULL");
    printf("get_filename(\"/test\") = %s\n", get_filename("/test") ? get_filename("/test") : "NULL");
    printf("get_filename(\"test\") = %s\n", get_filename("test") ? get_filename("test") : "NULL");
    return 0;
}
