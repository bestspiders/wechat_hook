#include <Windows.h>
#include <stdio.h>
#include <time.h> 
//createmutexw断点
//00D23135  
//7556F97B   .  8BD8          mov ebx,eax
//7577F9B0 >    8BFF          mov edi,edi    7577F9B0 >8B FF 55 8B EC 33 C0 39 45 0C 68 01 00 1F 00 0F   
//7577F97B   .  8BD8          mov ebx,eax    7577F97B  8B D8 89 1D 38 66 85 75 8B 45 D4 85 DB 75 8D EB  


//仿od断在入口
//0050312B > $  E8 2E040000   call WeChat.0050355E
#define call_point 0x50312B
//第一次断点
#define break_point 0x7577F97B
//第二次断点
#define weixin_break 0x7577F9B0


//全局调试句柄
HANDLE g_hProcess = NULL;
HANDLE g_hThread = NULL;
HANDLE dwOldbyte = NULL;
HANDLE dwINT3code = NULL;
HANDLE int3 = NULL;
HANDLE push_entrace= NULL;
int main() {
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = { 0 };

    if (CreateProcess(
        TEXT("C:\\Program Files (x86)\\Tencent\\WeChat\\WeChat.exe"),
        NULL,
        NULL,
        NULL,
        FALSE,
        DEBUG_ONLY_THIS_PROCESS ,    //| CREATE_NEW_CONSOLE
        NULL,
        NULL,
        &si,
        &pi) == FALSE) {
        int error_number = GetLastError();
        printf("%d\n",error_number);
        return 1;
    }
    g_hProcess = pi.hProcess;
    g_hThread = pi.hThread;
    BOOL waitEvent = TRUE;
    DEBUG_EVENT debugEvent;
    BYTE dwOldbyte[1] = {0};
        //int 3断点
    BYTE       int3[1] = {0xCC};
    //再次读取的内存
    BYTE ReadBuffer[MAX_PATH]={0};
    DWORD  dwState;
    //11 23 45 67 85 00 43 00
    //BYTE write_content[] = {0x11,0x23,0x45,0x67,0x85,0x00,0x43,0x00};
    BYTE raw_list[]={0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x30};
    srand((unsigned)time(NULL));
    BYTE write_content[] = {0x11,0x23,0x45,raw_list[rand()%21],raw_list[rand()%21],raw_list[rand()%21],raw_list[rand()%21],raw_list[rand()%21]};
    DWORD wrtie_size = sizeof(write_content);
    //寄存器结构体
    CONTEXT Regs;
    Regs.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS;

    while (waitEvent == TRUE) {
        WaitForDebugEvent(&debugEvent, INFINITE);
        dwState = DBG_EXCEPTION_NOT_HANDLED;
        switch (debugEvent.dwDebugEventCode) {

            case CREATE_PROCESS_DEBUG_EVENT:
                //OnProcessCreated(&debugEvent.u.CreateProcessInfo);
                GetThreadContext(g_hThread,&Regs);
                //断在入口处
                ReadProcessMemory(g_hProcess,(LPCVOID)(call_point),&push_entrace,1,NULL);
                WriteProcessMemory(g_hProcess,(LPVOID)call_point,&int3,1,NULL);
                
                dwState =DBG_CONTINUE;
                printf("Debuggee was created.%x\n",Regs.Eip);
                SetThreadContext(g_hThread,&Regs);
                //printf("address:%x\n",&dwOldbyte);
                break;

            case CREATE_THREAD_DEBUG_EVENT:
                printf("create thread\n");
//                OnThreadCreated(&debugEvent.u.CreateThread);
                break;

            case EXCEPTION_DEBUG_EVENT:
//                printf("exception debug\n");
                switch (debugEvent.u.Exception.ExceptionRecord.ExceptionCode)
                {
                    case EXCEPTION_BREAKPOINT:
                    {
                        printf("exception code\n");
                        GetThreadContext(g_hThread,&Regs);
                        printf("%x\n",Regs.Eip);
                        if(Regs.Eip==break_point+1){
                            Regs.Eip--;
                            //恢复之前在createMutex之前的断点
                            WriteProcessMemory(g_hProcess,(LPVOID)break_point,&dwOldbyte,1,0);
                            //写入第二次断点
                            ReadProcessMemory(g_hProcess,(LPCVOID)(weixin_break),&dwOldbyte,1,NULL);
                            WriteProcessMemory(g_hProcess,(LPVOID)weixin_break,&int3,1,NULL);
                            printf("break point\n");
                            SetThreadContext(g_hThread,&Regs);
                        }else if(Regs.Eip==weixin_break+1){
                            Regs.Eip--;
                            //恢复第二次断点
                            WriteProcessMemory(g_hProcess,(LPVOID)weixin_break,&dwOldbyte,1,0);
                            //写入变量存储的位置
                            WriteProcessMemory(g_hProcess,(LPVOID)Regs.Esi,write_content,wrtie_size,NULL);
                            printf("Esi:%x",Regs.Esi);
                            SetThreadContext(g_hThread,&Regs);
                        }else if(Regs.Eip==call_point+1){
                            Regs.Eip--;
                            //恢复入口断点
                            WriteProcessMemory(g_hProcess,(LPVOID)call_point,&push_entrace,1,NULL);
                            printf("now it's entrace point.\n");
                            //断在createMutexW之前
                            ReadProcessMemory(g_hProcess,(LPCVOID)(break_point),&dwOldbyte,1,NULL);
                            WriteProcessMemory(g_hProcess,(LPVOID)break_point,&int3,1,NULL);
                            SetThreadContext(g_hThread,&Regs);
                        }
                        dwState=DBG_CONTINUE;
                        break;
                    }
                }
                //OnException(&debugEvent.u.Exception);
                break;

            case EXIT_PROCESS_DEBUG_EVENT:
//                OnProcessExited(&debugEvent.u.ExitProcess);
                waitEvent = FALSE;
                break;

            case EXIT_THREAD_DEBUG_EVENT:
//                OnThreadExited(&debugEvent.u.ExitThread);
                break;

            case LOAD_DLL_DEBUG_EVENT:
                // module = (HMODULE) debugEvent.u.LoadDll.lpBaseOfDll;
                // file = debugEvent.u.LoadDll.hFile;
                // if (get_file_name_from_handle(file,
                //     buf, sizeof buf) == -1) {
                //     return 1;
                // }
                // printf("LOAD_DLL   %p %s\n", module, buf);
                // char *s = strrchr(buf, '\\');
                // if (s == NULL) {
                //     s = buf;
                // } else {
                //     s++;
                // }
                // if (stricmp(s, argv[1]) == 0
                //     && install_dll_breakpoint(process, module, argv[1],
                //               argv[2]) == -1) {
                // return 1;
                // }
                // CloseHandle(file);
//                OnDllLoaded(&debugEvent.u.LoadDll);
                break;

            case UNLOAD_DLL_DEBUG_EVENT:
//                OnDllUnloaded(&debugEvent.u.UnloadDll);
                break;

            case OUTPUT_DEBUG_STRING_EVENT:
//                OnOutputDebugString(&debugEvent.u.DebugString);
                break;

            case RIP_EVENT:
//                OnRipEvent(&debugEvent.u.RipInfo);
                break;

            default:
                printf("Unknown debug event.\n");
                break;
        }

        if (waitEvent == TRUE) {
            ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
        }
        else {
            break;
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
