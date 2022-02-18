#pragma once

#include <vcclr.h>
#include <Windows.h>

#include "pe.hxx"
#include "injector.hxx"
#include "options.hxx"

namespace syringe {

using namespace System;
using namespace System::Text;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;
using namespace System::Diagnostics;
using namespace System::Globalization;

// clang-format off
#ifdef _WIN64
static constexpr pe::Type SYRINGE_EXE_TYPE = pe::Type::Pe32Plus;
#else
static constexpr pe::Type SYRINGE_EXE_TYPE = pe::Type::Pe32;
#endif

static String^ InjectionErrorToString(InjectionStatus status) {
    LPWSTR buffer;
    auto size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
                                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR) &buffer, 0, NULL);

    String^ msg;

    switch (status) {
    case InjectionStatus::ALLOC_FAILED:
        msg = L"Failed to allocate memory.";
        break;
    case InjectionStatus::CREATE_PROCESS_FAILED:
        msg = L"Failed to create victim process.";
        break;
    case InjectionStatus::OPEN_PROCESS_FAILED:
        msg = L"Failed to open victim process.";
        break;
    case InjectionStatus::PROC_ADDRESS_FAILED:
        msg = L"Failed to locate procedures in memory.";
        break;
    case InjectionStatus::REMOTE_ALLOC_FAILED:
        msg = L"Failed to allocate memory in victim process.";
        break;
    case InjectionStatus::WRITE_FAILED:
        msg = L"Failed to write payload path to victim process' memory.";
        break;
    case InjectionStatus::CREATE_THREAD_FAILED:
        msg = L"Failed to create thread in victim process.";
        break;
    case InjectionStatus::INIT_FAILED:
        msg = L"Failed to initialize payload (Init function failed).";
        break;
    default:
        msg = L"Injection failed with an unknown error.";
        break;
    }

    return String::Format("Injection failed.\n{0}\n{1}", msg, gcnew String(buffer, 0, (int) size));
}

public
ref class GUI : public System::Windows::Forms::Form {
    public:
    GUI(String^ dllPath, String^ programPath, String^ pid, String^ cmdLine, bool launchMode) {
        InitializeComponent();
        ReloadRunningProcesses();

        {
            auto moduleFile = Process::GetCurrentProcess()->MainModule->FileName;

            // If the other syringe version is not present, hide the 'Switch' button.
#ifdef _WIN64
            if (!moduleFile->EndsWith("syringe.exe") || !File::Exists("syringe32.exe")) {
                Menu->Items->Remove(SwitchArchitecture);
            }

            Text = "syringe64";
            SwitchArchitecture->Text = "Switch to 32-bit";
#else
            if (!moduleFile->EndsWith("syringe32.exe") || !File::Exists("syringe.exe")) {
                Menu->Items->Remove(SwitchArchitecture);
            }

            Text = "syringe32";
            SwitchArchitecture->Text = "Switch to 64-bit";
#endif
        }

        HANDLE token;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            TOKEN_ELEVATION elevation;
            DWORD           size;

            if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
                if (elevation.TokenIsElevated) {
                    Elevate->Text    = "Elevated";
                    Elevate->Enabled = false;
                }
            }

            CloseHandle(token);
        }

        // Hide the outline for disabled controls in the menu strip.
        Menu->Renderer = gcnew BetterToolStripMenuRenderer();

        OptionsGUI::COMMAND_LINE = cmdLine;

        if (!launchMode) {
            InjectMode->Checked = true;
        }

        if (dllPath) {
            DllPath->Text = dllPath;
        }

        if (programPath) {
            ProgramPath->Text = programPath;
        }

        if (pid) {
            RunningProcesses->SelectedIndex = -1;
            RunningProcesses->Text          = pid;
        }
    }

    private:
    bool     dllPathHasInfo = false;
    pe::Type dllType;

    bool     programPathHasInfo = false;
    pe::Type programType;

    bool     pidHasInfo = false;
    pe::Type pidType;

    private:
    System::Windows::Forms::GroupBox^ PidGroup;

    private:
    System::Windows::Forms::Button^ RefreshRunningProcesses;

    private:
    System::Windows::Forms::MenuStrip^ Menu;

    private:
    System::Windows::Forms::ToolStripMenuItem^ Options;

    private:
    System::Windows::Forms::ToolStripMenuItem^ Elevate;

    private:
    System::Windows::Forms::Panel^ MenuPanel;

    private:
    System::Windows::Forms::OpenFileDialog^ DllPathDialog;

    private:
    System::Windows::Forms::OpenFileDialog^ ProgramPathDialog;

    private:
    System::Windows::Forms::ToolTip^ ToolTip;
private: System::Windows::Forms::ToolStripMenuItem^  SwitchArchitecture;

    private:
    private:
    private:
    System::Windows::Forms::ComboBox^ RunningProcesses;

    void CheckPePath(String^ path, GroupBox^ group, String^ title, bool % hasInfo, pe::Type % type) {
        if (!System::IO::File::Exists(path)) {
            if (hasInfo) {
                hasInfo     = false;
                group->Text = title;
                group->Refresh();
            }

            return;
        }

        type    = pe::GetType(path);
        hasInfo = true;
    }

    void CheckDllPath() {
        CheckPePath(DllPath->Text, DllGroup, gcnew String(L"DLL Path"), (bool&) dllPathHasInfo, (pe::Type&) dllType);
    }

    void CheckDllCompatibility() {
        if (!dllPathHasInfo) {
            DllGroup->ForeColor = Color::FromArgb(255, 0, 0, 0);
            return;
        }

        bool     victimHasInfo;
        pe::Type victimType;

        if (LaunchMode->Checked) {
            victimHasInfo = programPathHasInfo;
            victimType    = programType;
        } else {
            victimHasInfo = pidHasInfo;
            victimType    = pidType;
        }

        if (dllType == pe::Type::Invalid) {
            DllGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
        } else {
            if (dllType != SYRINGE_EXE_TYPE) {
                DllGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
                DllGroup->Text      = String::Format(L"DLL Path (must use {0}-bit syringe)", dllType == pe::Type::Pe32 ? L"32" : L"64");
                DllGroup->Refresh();
                return;
            }

            if (victimHasInfo) {
                if (victimType == pe::Type::Invalid || victimType == dllType) {
                    DllGroup->ForeColor = Color::FromArgb(255, 0, 128, 0);
                } else {
                    DllGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
                    DllGroup->Text      = String::Format(L"DLL Path ({0}-bit: must be {1}-bit)", dllType == pe::Type::Pe32 ? L"32" : L"64",
                                                    victimType == pe::Type::Pe32 ? L"32" : L"64");
                    DllGroup->Refresh();
                    return;
                }
            } else {
                DllGroup->ForeColor = Color::FromArgb(255, 0, 128, 0);
            }
        }

        switch (dllType) {
        case pe::Type::Pe32:
            DllGroup->Text = L"DLL Path (32-bit detected)";
            break;
        case pe::Type::Pe32Plus:
            DllGroup->Text = L"DLL Path (64-bit detected)";
            break;
        default:
            DllGroup->Text = L"DLL Path (invalid)";
            break;
        }

        DllGroup->Refresh();
    }

    void CheckProgramPath() {
        CheckPePath(ProgramPath->Text, ProgramGroup, gcnew String(L"Program Path"), (bool&) programPathHasInfo, (pe::Type&) programType);
    }

    void CheckProgramCompatibility() {
        if (!programPathHasInfo) {
            ProgramGroup->ForeColor = Color::FromArgb(255, 0, 0, 0);
            return;
        }

        if (programType == pe::Type::Invalid) {
            ProgramGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
        } else {
            ProgramGroup->ForeColor = Color::FromArgb(255, 0, 128, 0);
        }

        switch (programType) {
        case pe::Type::Pe32:
            ProgramGroup->Text = L"Program Path (32-bit detected)";
            break;
        case pe::Type::Pe32Plus:
            ProgramGroup->Text = L"Program Path (64-bit detected)";
            break;
        default:
            ProgramGroup->Text = L"Program Path (invalid)";
            break;
        }

        DllGroup->Refresh();
    }

    void CheckPid() {
        try {
            pidHasInfo = true;

            auto pidString = RunningProcesses->Text->Trim()->Split(' ')[0];

            int pid; // Since Process.Id is of type int, we use int instead of DWORD.
            if (!Int32::TryParse(pidString, NumberStyles::Integer, CultureInfo::InvariantCulture, pid)) {
                PidGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
                PidGroup->Text      = L"Process ID (invalid)";
                PidGroup->Refresh();
                pidType = pe::Type::Invalid;
                return;
            }

            auto process = Process::GetProcessById(pid);

            if (!process) {
                PidGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
                PidGroup->Text      = L"Process ID (invalid)";
                PidGroup->Refresh();
                pidType = pe::Type::Invalid;
                return;
            }

            BOOL isWow64;
            if (!IsWow64Process((HANDLE) process->Handle, &isWow64)) {
                PidGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
                PidGroup->Text      = L"Process ID (failed to detect)";
                PidGroup->Refresh();
                pidType = pe::Type::Invalid;
                return;
            }

            if (isWow64) {
                // 32 bit
                pidType             = pe::Type::Pe32;
                PidGroup->ForeColor = Color::FromArgb(255, 0, 128, 0);
                PidGroup->Text      = L"Process ID (32-bit detected)";
            } else {
                SYSTEM_INFO info;
                GetNativeSystemInfo(&info);

                if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
                    // 32 bit
                    pidType             = pe::Type::Pe32;
                    PidGroup->ForeColor = Color::FromArgb(255, 0, 128, 0);
                    PidGroup->Text      = L"Process ID (32-bit detected)";
                } else {
                    // 64 bit
                    pidType             = pe::Type::Pe32Plus;
                    PidGroup->ForeColor = Color::FromArgb(255, 0, 128, 0);
                    PidGroup->Text      = L"Process ID (64-bit detected)";
                }
            }
        } catch (Exception^) {
            PidGroup->ForeColor = Color::FromArgb(255, 128, 0, 0);
            PidGroup->Text      = L"Process ID (failed to detect)";
            PidGroup->Refresh();
            pidType = pe::Type::Invalid;
            return;
        }
    }

    void UpdateInjectButtonText() {
        if (LaunchMode->Checked) {
            Inject->Text = L"Launch && Inject";
        } else {
            Inject->Text = L"Inject";
        }

        Inject->Refresh();
    }

    int CompareProcesses(Process^ left, Process^ right) {
        return String::Compare(left->ProcessName, right->ProcessName, StringComparison::InvariantCultureIgnoreCase);
    }

    void ReloadRunningProcesses() {
        RunningProcesses->Items->Clear();

        auto processes = Process::GetProcesses();

        Array::Sort(processes, gcnew Comparison<Process^>(this, &GUI::CompareProcesses));

        for (int i = 0; i < processes->Length; ++i) {
            auto title = !String::IsNullOrWhiteSpace(processes[i]->MainWindowTitle) ? String::Format(" - {0}", processes[i]->MainWindowTitle) : "";
            RunningProcesses->Items->Add(String::Format("{0} - {1}{2}", processes[i]->Id, processes[i]->ProcessName, title));
        }

        if (RunningProcesses->Items->Count == 0) {
            RunningProcesses->Items->Add("No processes found");
        }

        RunningProcesses->SelectedIndex = 0;
    }

    void ShellExecuteSyringe(LPCWSTR command, LPCWSTR exe) {
        pin_ptr<const wchar_t> wd  = PtrToStringChars(Environment::CurrentDirectory);

        auto builder = gcnew StringBuilder();

        //builder->AppendFormat("\"{0}\"", Process::GetCurrentProcess()->MainModule->FileName);
        builder->Append("--reserved-launch");

        builder->Append(LaunchMode->Checked ? " 1" : " 0");

        if (!String::IsNullOrWhiteSpace(DllPath->Text)) {
            builder->AppendFormat(" \"{0}\"", DllPath->Text);
        } else {
            builder->Append(" \"\"");
        }

        if (!String::IsNullOrWhiteSpace(ProgramPath->Text)) {
            builder->AppendFormat(" \"{0}\"", ProgramPath->Text);
        } else {
            builder->Append(" \"\"");
        }

        if (!String::IsNullOrWhiteSpace(RunningProcesses->Text)) {
            builder->AppendFormat(" \"{0}\"", RunningProcesses->Text);
        } else {
            builder->Append(" \"\"");
        }

        if (!String::IsNullOrWhiteSpace(OptionsGUI::COMMAND_LINE)) {
            builder->AppendFormat(" \"{0}\"", OptionsGUI::COMMAND_LINE);
        } else {
            builder->Append(" \"\"");
        }

        pin_ptr<const wchar_t> args = PtrToStringChars(builder->ToString());

        if (32 < (INT_PTR) ShellExecuteW((HWND) Handle.ToPointer(), command, exe, args, wd, SW_SHOWNORMAL)) {
            // Success
            ExitProcess(0);
        }
    }

    System::Void Options_Click(System::Object^ sender, System::EventArgs^ e) {
        auto options           = gcnew OptionsGUI();
        options->StartPosition = FormStartPosition::Manual;
        options->Left          = Location.X + Size.Width / 2 - options->Size.Width / 2;
        options->Top           = Location.Y + Size.Height / 2 - options->Size.Height / 2;
        options->ShowDialog();
    }

    System::Void Elevate_Click(System::Object^ sender, System::EventArgs^ e) {
        pin_ptr<const wchar_t> exe = PtrToStringChars(Process::GetCurrentProcess()->MainModule->FileName);

        ShellExecuteSyringe(L"runas", exe);
    }

    private: System::Void SwitchArchitecture_Click(System::Object^  sender, System::EventArgs^  e) {
        auto moduleFile = Process::GetCurrentProcess()->MainModule->FileName;

#ifdef _WIN64
            // syringe.exe -> syringe32.exe
            moduleFile = moduleFile->Substring(0, moduleFile->Length - 4) + "32.exe";
#else
            // syringe32.exe -> syringe.exe
            moduleFile = moduleFile->Substring(0, moduleFile->Length - 6) + ".exe";
#endif

        pin_ptr<const wchar_t> exe = PtrToStringChars(moduleFile);

        // If syringe is elevated, run the new instance as admin as well
        ShellExecuteSyringe(Elevate->Enabled ? L"open" : L"runas", exe);
    }

    System::Void DllSelect_Click(System::Object^ sender, System::EventArgs^ e) {
        if (DllPathDialog->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
            DllPath->Text = DllPathDialog->FileName;
        }
    }

    System::Void ProgramSelect_Click(System::Object^ sender, System::EventArgs^ e) {
        if (ProgramPathDialog->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
            ProgramPath->Text = ProgramPathDialog->FileName;
        }
    }

    System::Void DllPath_TextChanged(System::Object^ sender, System::EventArgs^ e) {
        CheckDllPath();
        CheckDllCompatibility();
    }

    System::Void ProgramPath_TextChanged(System::Object^ sender, System::EventArgs^ e) {
        CheckProgramPath();
        CheckProgramCompatibility();
        CheckDllCompatibility();
    }

    System::Void RunningProcesses_TextUpdate(System::Object^ sender, System::EventArgs^ e) {
        CheckPid();
        CheckDllCompatibility();
    }

    System::Void LaunchMode_CheckedChanged(System::Object^ sender, System::EventArgs^ e) {
        if (LaunchMode->Checked) {
            ProgramGroup->Show();
            PidGroup->Hide();
            Inject->Text = L"Launch && Inject";
        } else {
            ProgramGroup->Hide();
            PidGroup->Show();
            Inject->Text = L"Inject";
        }

        ProgramGroup->Refresh();
        PidGroup->Refresh();
        Inject->Refresh();

        CheckDllCompatibility();
    }

    System::Void RefreshRunningProcesses_Click(System::Object^ sender, System::EventArgs^ e) {
        ReloadRunningProcesses();
    }

    System::Void Inject_Click(System::Object^ sender, System::EventArgs^ e) {
        CheckDllPath();

        if (dllType == pe::Type::Invalid) {
            Error("The DLL path doesn't point to a valid DLL file.\nPlease type the path to a valid DLL.");
            return;
        }

        InjectionStatus status;

        if (LaunchMode->Checked) {
            CheckProgramPath();
            CheckProgramCompatibility();
            CheckDllCompatibility();

            if (programType == pe::Type::Invalid) {
                Error("The program path doesn't point to a valid PE file.\nPlease type the path to a valid program.");
                return;
            }

            if (dllType != SYRINGE_EXE_TYPE) {
                Error(String::Format("Cannot inject {0}-bit DLL with the {1}-bit version of syringe.\nPlease use syringe {0}-bit.",
                    (dllType == pe::Type::Pe32 ? "32" : "64"), (SYRINGE_EXE_TYPE == pe::Type::Pe32 ? "32" : "64")));
                return;
            }

            if (dllType != programType) {
                Error(String::Format("Cannot inject {0}-bit DLL into {1}-bit program.\nThe DLL and program architectures must match.",
                                     (dllType == pe::Type::Pe32 ? "32" : "64"), (programType == pe::Type::Pe32 ? "32" : "64")));
                return;
            }

            pin_ptr<const wchar_t> programStr = PtrToStringChars(ProgramPath->Text);
            pin_ptr<const wchar_t> dllStr     = PtrToStringChars(DllPath->Text);

            auto cmdLineBuilder = gcnew StringBuilder();

            cmdLineBuilder->AppendFormat("\"{0}\"", ProgramPath->Text);

            if (!String::IsNullOrWhiteSpace(OptionsGUI::COMMAND_LINE)) {
                cmdLineBuilder->AppendFormat(" \"{0}\"", OptionsGUI::COMMAND_LINE);
            }

            pin_ptr<const wchar_t> cmdLine = PtrToStringChars(cmdLineBuilder->ToString());

            Inject->Text = L"Injecting...";
            Inject->Refresh();

            status = inject_start(programStr, cmdLine, dllStr, NULL);
        } else {
            CheckPid();
            CheckDllCompatibility();

            auto pidString = RunningProcesses->Text->Trim()->Split(' ')[0];

            int pid; // Since Process.Id is of type int, we use int instead of DWORD.
            if (!Int32::TryParse(pidString, NumberStyles::Integer, CultureInfo::InvariantCulture, pid)) {
                Error(String::Format("Invalid running process ID (must be valid int32).\nPlease type a valid ID or choose a process from the list.", pid));
                return;
            }

            try {
                auto process = Process::GetProcessById(pid);

                if (!process) {
                    throw gcnew Exception();
                }
            } catch (Exception^) {
                Error(String::Format("Invalid process ID {0}.\nNo running process with this ID exists.", pidString));
                return;
            }

            if (pidType == pe::Type::Invalid) {
                Error("Cannot get information about the target process.\nTry running syringe with administrator privileges.");
                return;
            }

            if (dllType != pidType) {
                Error(String::Format("Cannot inject {0}-bit DLL into {1}-bit process.\nThe DLL and process architectures must match.",
                                     (dllType == pe::Type::Pe32 ? "32" : "64"), (pidType == pe::Type::Pe32 ? "32" : "64")));
                return;
            }

            pin_ptr<const wchar_t> dllStr = PtrToStringChars(DllPath->Text);

            Inject->Text = L"Injecting...";
            Inject->Refresh();

            status = inject((DWORD) pid, dllStr);
        }

        if (status == InjectionStatus::OK) {
            UpdateInjectButtonText();
            return;
        }

        auto msg = InjectionErrorToString(status);

        // Don't make the call earlier because GetLastError must reflect the inject() error,
        // and the last error may be overwritten by this call.
        UpdateInjectButtonText();

        Error(msg);
    }

    void Error(String^ msg) {
        MessageBox::Show(msg, "syringe error", MessageBoxButtons::OK, MessageBoxIcon::Error);
    }

    protected:
    /// <summary>
    /// Clean up any resources being used.
    /// </summary>
    ~GUI() {
        if (components) {
            delete components;
        }
    }

    private:
    System::Windows::Forms::GroupBox^ ModeGroup;

    protected:
    private:
    System::Windows::Forms::RadioButton^ InjectMode;

    private:
    System::Windows::Forms::RadioButton^ LaunchMode;

    private:
    System::Windows::Forms::GroupBox^ DllGroup;

    private:
    System::Windows::Forms::Button^ DllSelect;

    private:
    System::Windows::Forms::TextBox^ DllPath;

    private:
    System::Windows::Forms::GroupBox^ ProgramGroup;

    private:
    System::Windows::Forms::Button^ ProgramSelect;

    private:
    System::Windows::Forms::TextBox^ ProgramPath;

    private:
    System::Windows::Forms::Button^ Inject;

    private:
    System::ComponentModel::IContainer^ components;

    private:
    /// <summary>
    /// Required designer variable.
    /// </summary>


    ref class BetterToolStripMenuRenderer : ToolStripProfessionalRenderer {
        protected:
        void OnRenderMenuItemBackground(ToolStripItemRenderEventArgs^ e) override {
            if (e->Item->Enabled) {
                ToolStripProfessionalRenderer::OnRenderMenuItemBackground(e);
            }
        }

        void OnRenderButtonBackground(ToolStripItemRenderEventArgs^ e) override {
            if (e->Item->Enabled) {
                ToolStripProfessionalRenderer::OnRenderMenuItemBackground(e);
            }
        }
    };

#pragma region Windows Form Designer generated code
    /// <summary>
    /// Required method for Designer support - do not modify
    /// the contents of this method with the code editor.
    /// </summary>
    void InitializeComponent(void) {
        this->components = (gcnew System::ComponentModel::Container());
        System::ComponentModel::ComponentResourceManager^  resources = (gcnew System::ComponentModel::ComponentResourceManager(GUI::typeid));
        this->ModeGroup = (gcnew System::Windows::Forms::GroupBox());
        this->InjectMode = (gcnew System::Windows::Forms::RadioButton());
        this->LaunchMode = (gcnew System::Windows::Forms::RadioButton());
        this->DllGroup = (gcnew System::Windows::Forms::GroupBox());
        this->DllSelect = (gcnew System::Windows::Forms::Button());
        this->DllPath = (gcnew System::Windows::Forms::TextBox());
        this->ProgramGroup = (gcnew System::Windows::Forms::GroupBox());
        this->ProgramSelect = (gcnew System::Windows::Forms::Button());
        this->ProgramPath = (gcnew System::Windows::Forms::TextBox());
        this->Inject = (gcnew System::Windows::Forms::Button());
        this->PidGroup = (gcnew System::Windows::Forms::GroupBox());
        this->RefreshRunningProcesses = (gcnew System::Windows::Forms::Button());
        this->RunningProcesses = (gcnew System::Windows::Forms::ComboBox());
        this->Menu = (gcnew System::Windows::Forms::MenuStrip());
        this->Options = (gcnew System::Windows::Forms::ToolStripMenuItem());
        this->Elevate = (gcnew System::Windows::Forms::ToolStripMenuItem());
        this->MenuPanel = (gcnew System::Windows::Forms::Panel());
        this->DllPathDialog = (gcnew System::Windows::Forms::OpenFileDialog());
        this->ProgramPathDialog = (gcnew System::Windows::Forms::OpenFileDialog());
        this->ToolTip = (gcnew System::Windows::Forms::ToolTip(this->components));
        this->SwitchArchitecture = (gcnew System::Windows::Forms::ToolStripMenuItem());
        this->ModeGroup->SuspendLayout();
        this->DllGroup->SuspendLayout();
        this->ProgramGroup->SuspendLayout();
        this->PidGroup->SuspendLayout();
        this->Menu->SuspendLayout();
        this->MenuPanel->SuspendLayout();
        this->SuspendLayout();
        // 
        // ModeGroup
        // 
        this->ModeGroup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->ModeGroup->Controls->Add(this->InjectMode);
        this->ModeGroup->Controls->Add(this->LaunchMode);
        this->ModeGroup->Location = System::Drawing::Point(12, 27);
        this->ModeGroup->Name = L"ModeGroup";
        this->ModeGroup->Size = System::Drawing::Size(202, 70);
        this->ModeGroup->TabIndex = 0;
        this->ModeGroup->TabStop = false;
        this->ModeGroup->Text = L"Mode";
        // 
        // InjectMode
        // 
        this->InjectMode->AutoSize = true;
        this->InjectMode->Location = System::Drawing::Point(6, 42);
        this->InjectMode->Name = L"InjectMode";
        this->InjectMode->Size = System::Drawing::Size(172, 17);
        this->InjectMode->TabIndex = 1;
        this->InjectMode->Text = L"Inject DLL into running process";
        this->ToolTip->SetToolTip(this->InjectMode, L"Inject the DLL into an already running process by PID");
        this->InjectMode->UseVisualStyleBackColor = true;
        // 
        // LaunchMode
        // 
        this->LaunchMode->AutoSize = true;
        this->LaunchMode->Checked = true;
        this->LaunchMode->Location = System::Drawing::Point(6, 19);
        this->LaunchMode->Name = L"LaunchMode";
        this->LaunchMode->Size = System::Drawing::Size(187, 17);
        this->LaunchMode->TabIndex = 0;
        this->LaunchMode->TabStop = true;
        this->LaunchMode->Text = L"Launch program with injected DLL";
        this->ToolTip->SetToolTip(this->LaunchMode, L"Load a program, inject the DLL, and start the program execution");
        this->LaunchMode->UseVisualStyleBackColor = true;
        this->LaunchMode->CheckedChanged += gcnew System::EventHandler(this, &GUI::LaunchMode_CheckedChanged);
        // 
        // DllGroup
        // 
        this->DllGroup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->DllGroup->Controls->Add(this->DllSelect);
        this->DllGroup->Controls->Add(this->DllPath);
        this->DllGroup->Location = System::Drawing::Point(12, 103);
        this->DllGroup->Name = L"DllGroup";
        this->DllGroup->Size = System::Drawing::Size(202, 47);
        this->DllGroup->TabIndex = 1;
        this->DllGroup->TabStop = false;
        this->DllGroup->Text = L"DLL Path";
        this->ToolTip->SetToolTip(this->DllGroup, L"The path to the DLL to inject");
        // 
        // DllSelect
        // 
        this->DllSelect->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
        this->DllSelect->Image = (cli::safe_cast<System::Drawing::Image^  >(resources->GetObject(L"DllSelect.Image")));
        this->DllSelect->Location = System::Drawing::Point(167, 13);
        this->DllSelect->Name = L"DllSelect";
        this->DllSelect->Size = System::Drawing::Size(29, 26);
        this->DllSelect->TabIndex = 1;
        this->ToolTip->SetToolTip(this->DllSelect, L"Browse DLL file");
        this->DllSelect->UseVisualStyleBackColor = true;
        this->DllSelect->Click += gcnew System::EventHandler(this, &GUI::DllSelect_Click);
        // 
        // DllPath
        // 
        this->DllPath->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->DllPath->Location = System::Drawing::Point(6, 19);
        this->DllPath->Name = L"DllPath";
        this->DllPath->Size = System::Drawing::Size(155, 20);
        this->DllPath->TabIndex = 0;
        this->ToolTip->SetToolTip(this->DllPath, L"The path to the DLL to inject");
        this->DllPath->TextChanged += gcnew System::EventHandler(this, &GUI::DllPath_TextChanged);
        // 
        // ProgramGroup
        // 
        this->ProgramGroup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->ProgramGroup->Controls->Add(this->ProgramSelect);
        this->ProgramGroup->Controls->Add(this->ProgramPath);
        this->ProgramGroup->Location = System::Drawing::Point(12, 156);
        this->ProgramGroup->Name = L"ProgramGroup";
        this->ProgramGroup->Size = System::Drawing::Size(202, 47);
        this->ProgramGroup->TabIndex = 2;
        this->ProgramGroup->TabStop = false;
        this->ProgramGroup->Text = L"Program Path";
        this->ToolTip->SetToolTip(this->ProgramGroup, L"The path to the program to launch");
        // 
        // ProgramSelect
        // 
        this->ProgramSelect->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
        this->ProgramSelect->Image = (cli::safe_cast<System::Drawing::Image^  >(resources->GetObject(L"ProgramSelect.Image")));
        this->ProgramSelect->Location = System::Drawing::Point(167, 13);
        this->ProgramSelect->Name = L"ProgramSelect";
        this->ProgramSelect->Size = System::Drawing::Size(29, 26);
        this->ProgramSelect->TabIndex = 1;
        this->ToolTip->SetToolTip(this->ProgramSelect, L"Browse program executable");
        this->ProgramSelect->UseVisualStyleBackColor = true;
        this->ProgramSelect->Click += gcnew System::EventHandler(this, &GUI::ProgramSelect_Click);
        // 
        // ProgramPath
        // 
        this->ProgramPath->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->ProgramPath->Location = System::Drawing::Point(6, 19);
        this->ProgramPath->Name = L"ProgramPath";
        this->ProgramPath->Size = System::Drawing::Size(155, 20);
        this->ProgramPath->TabIndex = 0;
        this->ToolTip->SetToolTip(this->ProgramPath, L"The path to the program to launch");
        this->ProgramPath->TextChanged += gcnew System::EventHandler(this, &GUI::ProgramPath_TextChanged);
        // 
        // Inject
        // 
        this->Inject->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->Inject->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
            static_cast<System::Byte>(0)));
        this->Inject->Image = (cli::safe_cast<System::Drawing::Image^  >(resources->GetObject(L"Inject.Image")));
        this->Inject->Location = System::Drawing::Point(12, 209);
        this->Inject->Name = L"Inject";
        this->Inject->Size = System::Drawing::Size(202, 55);
        this->Inject->TabIndex = 3;
        this->Inject->Text = L"Launch && Inject";
        this->Inject->TextAlign = System::Drawing::ContentAlignment::MiddleRight;
        this->Inject->TextImageRelation = System::Windows::Forms::TextImageRelation::ImageBeforeText;
        this->ToolTip->SetToolTip(this->Inject, L"Start the injection");
        this->Inject->UseVisualStyleBackColor = true;
        this->Inject->Click += gcnew System::EventHandler(this, &GUI::Inject_Click);
        // 
        // PidGroup
        // 
        this->PidGroup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->PidGroup->Controls->Add(this->RefreshRunningProcesses);
        this->PidGroup->Controls->Add(this->RunningProcesses);
        this->PidGroup->Location = System::Drawing::Point(12, 156);
        this->PidGroup->Name = L"PidGroup";
        this->PidGroup->Size = System::Drawing::Size(202, 47);
        this->PidGroup->TabIndex = 4;
        this->PidGroup->TabStop = false;
        this->PidGroup->Text = L"Running Process";
        this->ToolTip->SetToolTip(this->PidGroup, L"The PID of the process to inject the DLL into");
        this->PidGroup->Visible = false;
        // 
        // RefreshRunningProcesses
        // 
        this->RefreshRunningProcesses->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
        this->RefreshRunningProcesses->Image = (cli::safe_cast<System::Drawing::Image^  >(resources->GetObject(L"RefreshRunningProcesses.Image")));
        this->RefreshRunningProcesses->Location = System::Drawing::Point(167, 13);
        this->RefreshRunningProcesses->Name = L"RefreshRunningProcesses";
        this->RefreshRunningProcesses->Size = System::Drawing::Size(29, 26);
        this->RefreshRunningProcesses->TabIndex = 2;
        this->ToolTip->SetToolTip(this->RefreshRunningProcesses, L"Refresh list of running processes");
        this->RefreshRunningProcesses->UseVisualStyleBackColor = true;
        this->RefreshRunningProcesses->Click += gcnew System::EventHandler(this, &GUI::RefreshRunningProcesses_Click);
        // 
        // RunningProcesses
        // 
        this->RunningProcesses->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Left) 
                    | System::Windows::Forms::AnchorStyles::Right));
        this->RunningProcesses->AutoCompleteMode = System::Windows::Forms::AutoCompleteMode::SuggestAppend;
        this->RunningProcesses->AutoCompleteSource = System::Windows::Forms::AutoCompleteSource::ListItems;
        this->RunningProcesses->FormattingEnabled = true;
        this->RunningProcesses->Location = System::Drawing::Point(6, 18);
        this->RunningProcesses->Name = L"RunningProcesses";
        this->RunningProcesses->Size = System::Drawing::Size(155, 21);
        this->RunningProcesses->TabIndex = 0;
        this->ToolTip->SetToolTip(this->RunningProcesses, L"The PID of the process to inject the DLL into");
        this->RunningProcesses->SelectedIndexChanged += gcnew System::EventHandler(this, &GUI::RunningProcesses_TextUpdate);
        this->RunningProcesses->TextUpdate += gcnew System::EventHandler(this, &GUI::RunningProcesses_TextUpdate);
        // 
        // Menu
        // 
        this->Menu->BackColor = System::Drawing::SystemColors::ControlLightLight;
        this->Menu->Dock = System::Windows::Forms::DockStyle::Fill;
        this->Menu->GripMargin = System::Windows::Forms::Padding(0);
        this->Menu->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(3) {this->Options, this->Elevate, 
                this->SwitchArchitecture});
        this->Menu->Location = System::Drawing::Point(0, 0);
        this->Menu->Name = L"Menu";
        this->Menu->Padding = System::Windows::Forms::Padding(0);
        this->Menu->Size = System::Drawing::Size(226, 20);
        this->Menu->TabIndex = 5;
        this->Menu->Text = L"Menu";
        // 
        // Options
        // 
        this->Options->Name = L"Options";
        this->Options->Size = System::Drawing::Size(61, 20);
        this->Options->Text = L"Options";
        this->Options->ToolTipText = L"Other injection options like command line args";
        this->Options->Click += gcnew System::EventHandler(this, &GUI::Options_Click);
        // 
        // Elevate
        // 
        this->Elevate->Alignment = System::Windows::Forms::ToolStripItemAlignment::Right;
        this->Elevate->Name = L"Elevate";
        this->Elevate->Size = System::Drawing::Size(56, 20);
        this->Elevate->Text = L"Elevate";
        this->Elevate->ToolTipText = L"Elevate syringe to give it admin privileges";
        this->Elevate->Click += gcnew System::EventHandler(this, &GUI::Elevate_Click);
        // 
        // MenuPanel
        // 
        this->MenuPanel->Controls->Add(this->Menu);
        this->MenuPanel->Dock = System::Windows::Forms::DockStyle::Top;
        this->MenuPanel->Location = System::Drawing::Point(0, 0);
        this->MenuPanel->Name = L"MenuPanel";
        this->MenuPanel->Size = System::Drawing::Size(226, 20);
        this->MenuPanel->TabIndex = 6;
        // 
        // DllPathDialog
        // 
        this->DllPathDialog->Filter = L"DLL files|*.dll|All files|*.*";
        // 
        // ProgramPathDialog
        // 
        this->ProgramPathDialog->Filter = L"Executable files|*.exe|All files|*.*";
        // 
        // ToolTip
        // 
        this->ToolTip->AutomaticDelay = 250;
        this->ToolTip->AutoPopDelay = 10000;
        this->ToolTip->InitialDelay = 250;
        this->ToolTip->ReshowDelay = 50;
        // 
        // SwitchArchitecture
        // 
        this->SwitchArchitecture->Alignment = System::Windows::Forms::ToolStripItemAlignment::Right;
        this->SwitchArchitecture->Name = L"SwitchArchitecture";
        this->SwitchArchitecture->Size = System::Drawing::Size(102, 20);
        this->SwitchArchitecture->Text = L"Switch to 32-bit";
        this->SwitchArchitecture->ToolTipText = L"Switch syringe architecture";
        this->SwitchArchitecture->Click += gcnew System::EventHandler(this, &GUI::SwitchArchitecture_Click);
        // 
        // GUI
        // 
        this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
        this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
        this->ClientSize = System::Drawing::Size(226, 276);
        this->Controls->Add(this->MenuPanel);
        this->Controls->Add(this->Inject);
        this->Controls->Add(this->DllGroup);
        this->Controls->Add(this->ModeGroup);
        this->Controls->Add(this->PidGroup);
        this->Controls->Add(this->ProgramGroup);
        this->Icon = (cli::safe_cast<System::Drawing::Icon^  >(resources->GetObject(L"$this.Icon")));
        this->MainMenuStrip = this->Menu;
        this->MaximizeBox = false;
        this->MaximumSize = System::Drawing::Size(100000000, 315);
        this->MinimumSize = System::Drawing::Size(242, 315);
        this->Name = L"GUI";
        this->Text = L"syringe";
        this->ModeGroup->ResumeLayout(false);
        this->ModeGroup->PerformLayout();
        this->DllGroup->ResumeLayout(false);
        this->DllGroup->PerformLayout();
        this->ProgramGroup->ResumeLayout(false);
        this->ProgramGroup->PerformLayout();
        this->PidGroup->ResumeLayout(false);
        this->Menu->ResumeLayout(false);
        this->Menu->PerformLayout();
        this->MenuPanel->ResumeLayout(false);
        this->MenuPanel->PerformLayout();
        this->ResumeLayout(false);
    }
#pragma endregion
};
} // namespace syringe
// clang-format on