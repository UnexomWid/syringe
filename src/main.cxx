#include "syringe.hxx"
#include "pe.hxx"

using namespace System;
using namespace System::IO;
using namespace System::Text;
using namespace System::Diagnostics;
using namespace System::Globalization;
using namespace System::Windows::Forms;
using namespace System::Collections::Generic;

enum class ExitCode : int {
    INVALID_ARGS         = 1,
    TOO_MANY_ARGS        = 2,
    INVALID_PROGRAM_PATH = 3,
    ARCH_MISMATCH        = 4,
    INVALID_PROCESS_ID   = 5,
    ACCESS_DENIED        = 6,
    NOT_ENOUGH_ARGS      = 7,
    INJECTION_FAILED     = 8
};

// clang-format off
void Error(String ^ msg, ExitCode exitStatus, bool noGui) {
    if (!noGui) {
        MessageBox::Show(msg, "syringe error", MessageBoxButtons::OK, MessageBoxIcon::Error);
    }

    System::Environment::Exit((int) exitStatus);
}

[System::STAThreadAttribute]
void main() {
    int             argc {};
    array<String^>^ argv;
    bool            noGui = false;

    {
        // Since the args given by the CLR don't seem to include the exe path as the first arg, get the command line manually.
        auto rawCmdLine = GetCommandLineW();
        auto rawArgv    = CommandLineToArgvW(rawCmdLine, &argc);

        // Consume the --no-gui flag
        if (argc > 1 && 0 == wcscmp(rawArgv[1], L"--no-gui")) {
            noGui = true;
            argv  = gcnew array<String^>(argc - 1);
        } else {
            argv = gcnew array<String^>(argc);
        }

        // Since the --no-gui flag will be skipped, use this index for argv
        // and 'i' for the raw argv
        int index = 0;

        for (int i = 0; i < argc; ++i) {
            if (noGui && i == 1) {
                continue;
            }

            argv[index] = gcnew String(rawArgv[i]);

            ++index;
        }

        if (noGui) {
            --argc;
        } else {
            // The message boxes will have ugly buttons otherwise
            Application::EnableVisualStyles();
        }
    }

    String^        dllPath     = nullptr;
    String^        programPath = nullptr;
    String^        pid         = nullptr;
    String^        cmdLine     = nullptr;
    StringBuilder^ args        = gcnew StringBuilder();
    bool           launch      = true;
    bool           elevate     = false; // See --reserved-elevate

    if (argv->Length > 1) {
        if (argv[1]->Equals("--reserved-launch")) {
            // Syringe was started by another instance after pressing the [Elevate] or [Switch] button, so restore all user input values
            // This argument is reserved; a normal user must never pass it

            if (argv->Length != 7) {
                Error(L"Invalid arguments\nUsage: syringe.exe <DLL_Path> [--launch | -l | --pid | -p] <Program_Path_Or_PID> [args...]",
                      ExitCode::INVALID_ARGS, noGui);
            }

            launch      = argv[2]->Equals("1");
            dllPath     = argv[3];
            programPath = argv[4];
            pid         = argv[5];
            cmdLine     = argv[6];

            elevate = true;
        } else {
            dllPath = argv[1];

            switch (argv->Length) {
            case 2:
                break;
            case 3:
                // If the argument is a number, check if a file with that name exists.
                // If the file doesn't exist, consider the number as being a process ID.
                try {
                    auto pidNum = Int32::Parse(argv[2], NumberStyles::Integer, CultureInfo::InvariantCulture);

                    if (File::Exists(argv[2])) {
                        // Program
                        programPath = argv[2];
                    } else {
                        // Runnng process
                        pid    = argv[2];
                        launch = false;
                    }
                } catch (Exception^) {
                    // Program
                    programPath = argv[2];
                }
                break;
            case 4:
                if (argv[2]->Equals(gcnew String(L"--launch")) || argv[2]->Equals(gcnew String(L"-l"))) {
                    // Program
                    programPath = argv[3];
                } else if (argv[2]->Equals(gcnew String(L"--pid")) || argv[2]->Equals(gcnew String(L"-p"))) {
                    // Running process
                    pid    = argv[3];
                    launch = false;
                } else {
                    // Launch mode + 1 arg
                    programPath = argv[3];
                    args->Append(argv[4]);
                }
                break;
            default:
                // More than 4 args; launch mode + args
                if (argv[2]->Equals(gcnew String(L"--pid")) || argv[2]->Equals(gcnew String(L"-p"))) {
                    Error(String::Format(L"Too many arguments for 'pid' mode; expected max 3, got '{0}'\nUsage: syringe.exe <DLL_Path> [--launch | -l | --pid | -p] <Program_Path_Or_PID> [args...]",
                                         argv->Length - 1),
                          ExitCode::TOO_MANY_ARGS, noGui);
                }

                auto i = 2;
                if (argv[2]->Equals(gcnew String(L"--launch")) || argv[2]->Equals(gcnew String(L"-l"))) {
                    ++i;
                }

                programPath = argv[i];

                ++i;

                for (; i < argv->Length; ++i) {
                    args->AppendFormat("{0} ", argv[i]);
                }

                args->Remove(args->Length - 1, 1); // Remove the trailing space

                break;
            }
        }
    }

    if (dllPath && !elevate) {
        if (programPath) {
            // Inject launch mode
            auto programType = pe::GetType(programPath);
            auto dllType     = pe::GetType(dllPath);

            if (programType == pe::Type::Invalid) {
                Error(L"The program path doesn't point to a valid PE file.", ExitCode::INVALID_PROGRAM_PATH, noGui);
            }

            if (dllType != syringe::SYRINGE_EXE_TYPE) {
                Error(String::Format("Cannot inject {0}-bit DLL with the {1}-bit version of syringe.\nPlease use syringe {0}-bit.",
                    (dllType == pe::Type::Pe32 ? "32" : "64"), (syringe::SYRINGE_EXE_TYPE == pe::Type::Pe32 ? "32" : "64")), ExitCode::ARCH_MISMATCH, noGui);
            }

            if (dllType != programType) {
                Error(String::Format("Cannot inject {0}-bit DLL into {1}-bit program.\nThe DLL and program architectures must match.",
                                     (dllType == pe::Type::Pe32 ? "32" : "64"), (programType == pe::Type::Pe32 ? "32" : "64")),
                      ExitCode::ARCH_MISMATCH, noGui);
                return;
            }

            pin_ptr<const wchar_t> programStr = PtrToStringChars(programPath);
            pin_ptr<const wchar_t> dllStr     = PtrToStringChars(dllPath);

            auto cmdLineBuilder = gcnew StringBuilder();

            cmdLineBuilder->AppendFormat("\"{0}\"", programPath);
            cmdLineBuilder->Append(args);

            pin_ptr<const wchar_t> cmdLineStr = PtrToStringChars(cmdLineBuilder->ToString());

            auto status = inject_start(programStr, cmdLineStr, dllStr, NULL);

            if (status != InjectionStatus::OK) {
                auto msg = syringe::InjectionErrorToString(status);
                Console::Error->WriteLine(msg);
            }

            System::Environment::Exit(0);
        } else if (pid) {
            // Inject pid mode
            auto dllType = pe::GetType(dllPath);

            int pidNum; // Since Process.Id is of type int, we use int instead of DWORD.
            if (!Int32::TryParse(pid, NumberStyles::Integer, CultureInfo::InvariantCulture, pidNum)) {
                Error("Invalid running process ID (must be valid int32).", ExitCode::INVALID_PROCESS_ID, noGui);
            }

            try {
                auto process = Process::GetProcessById(pidNum);
                auto pidType = pe::Type::Invalid;

                if (!process) {
                    throw gcnew Exception();
                }

                BOOL isWow64;
                if (!IsWow64Process((HANDLE) process->Handle, &isWow64)) {
                    Error("Failed to detect target process architecture.\nTry running syringe with administrator privileges.", ExitCode::ACCESS_DENIED, noGui);
                }

                if (isWow64) {
                    // 32 bit
                    pidType = pe::Type::Pe32;
                } else {
                    SYSTEM_INFO info;
                    GetNativeSystemInfo(&info);

                    if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
                        // 32 bit
                        pidType = pe::Type::Pe32;
                    } else {
                        // 64 bit
                        pidType = pe::Type::Pe32Plus;
                    }
                }

                if (pidType == pe::Type::Invalid) {
                    Error("Cannot get information about the target process.\nTry running syringe with administrator privileges.", ExitCode::ACCESS_DENIED, noGui);
                    return;
                }

                if (dllType != pidType) {
                    Error(String::Format("Cannot inject {0}-bit DLL into {1}-bit process.\nThe DLL and process architectures must match.",
                                         (dllType == pe::Type::Pe32 ? "32" : "64"), (pidType == pe::Type::Pe32 ? "32" : "64")),
                          ExitCode::ARCH_MISMATCH, noGui);
                    return;
                }

                pin_ptr<const wchar_t> dllStr = PtrToStringChars(dllPath);

                auto status = inject((DWORD) pidNum, dllStr);

                if (status != InjectionStatus::OK) {
                    auto msg = syringe::InjectionErrorToString(status);
                    Error(msg, ExitCode::INJECTION_FAILED, noGui);
                }

                System::Environment::Exit(0);
            } catch (Exception^) {
                Error(String::Format("Invalid process ID {0}.\nNo running process with this ID exists.", pid), ExitCode::INVALID_PROCESS_ID, noGui);
            }
        }
    }

    // If at least one argument is missing, spawn a window.
    // If the user doesn't want a GUI, exit.
    if (noGui) {
        System::Environment::Exit((int) ExitCode::NOT_ENOUGH_ARGS);
    }

    Application::SetCompatibleTextRenderingDefault(false);

    syringe::GUI gui(dllPath, programPath, pid, cmdLine, launch);
    Application::Run(%gui);
}
// clang-format on