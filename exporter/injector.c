// injector.c — CreateRemoteThread+LoadLibraryW. gcc -O2 injector.c -o injector.exe -luser32
#include <windows.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc < 3) { printf("usage: injector <pid> <dll_path>\n"); return 1; }
  DWORD pid = (DWORD)atoi(argv[1]);
  const char* dll = argv[2];

  HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                             PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                             FALSE, pid);
  if (!hProc) { printf("OpenProcess failed (%lu). Admin?\n", GetLastError()); return 2; }

  SIZE_T len = strlen(dll) + 1;
  void* rem = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!rem) { printf("VirtualAllocEx failed\n"); return 3; }
  if (!WriteProcessMemory(hProc, rem, dll, len, NULL)) { printf("WriteProcessMemory failed\n"); return 4; }

  FARPROC loadLib = (FARPROC)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
  HANDLE hTh = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, rem, 0, NULL);
  if (!hTh) { printf("CreateRemoteThread failed (%lu)\n", GetLastError()); return 5; }
  WaitForSingleObject(hTh, 10000);
  DWORD exitCode = 0; GetExitCodeThread(hTh, &exitCode);
  printf("LoadLibraryA -> module handle 0x%lX (0 = fail)\n", exitCode);
  CloseHandle(hTh); VirtualFreeEx(hProc, rem, 0, MEM_RELEASE); CloseHandle(hProc);
  return exitCode ? 0 : 6;
}
