// A loadable library which is deliberately not a LÔVE extension.
#ifdef _WIN32
__declspec(dllexport)
#endif
int not_an_extension(void) { return 0; }
