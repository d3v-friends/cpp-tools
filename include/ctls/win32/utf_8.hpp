// 윈도우에서 콘솔의 인코딩을 utf-8 로 인식하게 해준다.
// main.cpp 에서 한번 로딩해준다.

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
#endif
// main 시작할 때
#if defined(_WIN32)
::SetConsoleOutputCP(65001);
#endif