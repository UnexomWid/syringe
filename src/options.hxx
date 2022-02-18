#pragma once

namespace syringe {

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;

// clang-format off
public
ref class OptionsGUI : public System::Windows::Forms::Form {
    public:
    static String^ COMMAND_LINE = L"";

    OptionsGUI(void) {
        InitializeComponent();
        CommandLine->Text = COMMAND_LINE;
        CommandLine->Focus();

#ifdef _WIN64
        Text = "syringe64 - options";
#else
        Text = "syringe32 - options";
#endif
    }

    private:
    System::Void Ok_Click(System::Object^ sender, System::EventArgs^ e) {
        COMMAND_LINE = CommandLine->Text;
        Close();
    }

    System::Void Cancel_Click(System::Object^ sender, System::EventArgs^ e) {
        Close();
    }

    protected:
    ~OptionsGUI() {
        if (components) {
            delete components;
        }
    }

    private:
    System::Windows::Forms::GroupBox^ CommandLineGroup;

    protected:
    private:
    System::Windows::Forms::Button^ Cancel;

    private:
    System::Windows::Forms::Button^ Ok;

    private:
    System::Windows::Forms::TextBox^ CommandLine;

    private:
    System::Windows::Forms::ToolTip^ ToolTip;

    private:
    System::ComponentModel::IContainer^ components;

    private:
    /// <summary>
    /// Required designer variable.
    /// </summary>


#pragma region Windows Form Designer generated code
    /// <summary>
    /// Required method for Designer support - do not modify
    /// the contents of this method with the code editor.
    /// </summary>
    void InitializeComponent(void) {
        this->components                                             = (gcnew System::ComponentModel::Container());
        System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(OptionsGUI::typeid));
        this->CommandLineGroup                                       = (gcnew System::Windows::Forms::GroupBox());
        this->CommandLine                                            = (gcnew System::Windows::Forms::TextBox());
        this->Cancel                                                 = (gcnew System::Windows::Forms::Button());
        this->Ok                                                     = (gcnew System::Windows::Forms::Button());
        this->ToolTip                                                = (gcnew System::Windows::Forms::ToolTip(this->components));
        this->CommandLineGroup->SuspendLayout();
        this->SuspendLayout();
        //
        // CommandLineGroup
        //
        this->CommandLineGroup->Controls->Add(this->CommandLine);
        this->CommandLineGroup->Location = System::Drawing::Point(12, 12);
        this->CommandLineGroup->Name     = L"CommandLineGroup";
        this->CommandLineGroup->Size     = System::Drawing::Size(260, 48);
        this->CommandLineGroup->TabIndex = 0;
        this->CommandLineGroup->TabStop  = false;
        this->CommandLineGroup->Text     = L"Command line args (launch mode)";
        this->ToolTip->SetToolTip(this->CommandLineGroup, L"Arguments for the program that will be launched");
        //
        // CommandLine
        //
        this->CommandLine->Location = System::Drawing::Point(6, 19);
        this->CommandLine->Name     = L"CommandLine";
        this->CommandLine->Size     = System::Drawing::Size(248, 20);
        this->CommandLine->TabIndex = 0;
        this->ToolTip->SetToolTip(this->CommandLine, L"Arguments for the program that will be launched");
        //
        // Cancel
        //
        this->Cancel->DialogResult            = System::Windows::Forms::DialogResult::Cancel;
        this->Cancel->Location                = System::Drawing::Point(116, 66);
        this->Cancel->Name                    = L"Cancel";
        this->Cancel->Size                    = System::Drawing::Size(75, 23);
        this->Cancel->TabIndex                = 1;
        this->Cancel->Text                    = L"Cancel";
        this->Cancel->UseVisualStyleBackColor = true;
        this->Cancel->Click += gcnew System::EventHandler(this, &OptionsGUI::Cancel_Click);
        //
        // Ok
        //
        this->Ok->Location                = System::Drawing::Point(197, 66);
        this->Ok->Name                    = L"Ok";
        this->Ok->Size                    = System::Drawing::Size(75, 23);
        this->Ok->TabIndex                = 2;
        this->Ok->Text                    = L"Ok";
        this->Ok->UseVisualStyleBackColor = true;
        this->Ok->Click += gcnew System::EventHandler(this, &OptionsGUI::Ok_Click);
        //
        // ToolTip
        //
        this->ToolTip->AutomaticDelay = 250;
        this->ToolTip->AutoPopDelay   = 10000;
        this->ToolTip->InitialDelay   = 250;
        this->ToolTip->ReshowDelay    = 50;
        //
        // OptionsGUI
        //
        this->AcceptButton        = this->Ok;
        this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
        this->AutoScaleMode       = System::Windows::Forms::AutoScaleMode::Font;
        this->CancelButton        = this->Cancel;
        this->ClientSize          = System::Drawing::Size(284, 94);
        this->Controls->Add(this->Ok);
        this->Controls->Add(this->Cancel);
        this->Controls->Add(this->CommandLineGroup);
        this->Icon        = (cli::safe_cast<System::Drawing::Icon^>(resources->GetObject(L"$this.Icon")));
        this->MaximizeBox = false;
        this->MaximumSize = System::Drawing::Size(300, 133);
        this->MinimumSize = System::Drawing::Size(300, 133);
        this->Name        = L"OptionsGUI";
        this->Text        = L"syringe - options";
        this->CommandLineGroup->ResumeLayout(false);
        this->CommandLineGroup->PerformLayout();
        this->ResumeLayout(false);
    }
#pragma endregion
};
} // namespace syringe
// clang-format on