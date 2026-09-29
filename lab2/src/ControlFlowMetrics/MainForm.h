#pragma once
// ============================================================================
//  MainForm.h — метрики Джилба и Маккейба для программ на Perl.
//
//  Это C++/CLI — C++ с расширениями для .NET. Напоминание по синтаксису:
//    ref class X          — класс, живущий в управляемой куче .NET;
//    X^ p = gcnew X()     — «шляпка» ^ вместо * и gcnew вместо new: объект
//                           удалит сборщик мусора, delete не нужен;
//    p->Метод()           — обращение к членам через ->, как у указателя;
//    String^              — строка .NET; L"текст" — литерал в UTF-16;
//    %form                — взять «управляемый адрес» объекта (аналог &);
//    событие += gcnew EventHandler(this, &MainForm::Обработчик)
//                         — подписка на событие (клик по кнопке и т.п.).
//
//  Всю математику делает класс PerlControlFlowAnalyzer (обычный C++). Форма только
//  берёт текст из редактора, отдаёт его анализатору и показывает результат.
// ============================================================================

#include "PerlControlFlowAnalyzer.h"
#include <vcclr.h>      // PtrToStringChars: доступ к символам String^ без копирования
#include <string>
#include <vector>

namespace ControlFlowMetrics
{
    using namespace System;
    using namespace System::ComponentModel;
    using namespace System::Collections;
    using namespace System::Windows::Forms;
    using namespace System::Data;
    using namespace System::Drawing;
    using namespace System::IO;
    using namespace System::Runtime::InteropServices;

    // Две функции Windows (user32.dll), которых нет в .NET Framework
    private ref class Native abstract sealed
    {
    public:
        [DllImport("user32.dll")]
        static IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        static bool SetProcessDPIAware();
    };

    // String^ (.NET) -> std::wstring (C++): копируем символы
    static std::wstring ToStd(String^ s)
    {
        if (s == nullptr) return std::wstring();
        pin_ptr<const wchar_t> p = PtrToStringChars(s);   // «пришпилить» строку, чтобы GC её не двигал
        return std::wstring(p, s->Length);
    }

    // std::wstring (C++) -> String^ (.NET)
    static String^ ToNet(const std::wstring& w)
    {
        return gcnew String(w.c_str(), 0, static_cast<int>(w.size()));
    }

    /// <summary>Главное окно приложения</summary>
    public ref class MainForm : public System::Windows::Forms::Form
    {
    public:
        // initialFile — путь к файлу из командной строки (может быть nullptr)
        MainForm(String^ initialFile)
        {
            InitializeComponent();
            lastResult = nullptr;
            highlightActive = false;
            updatingGrid = false;
            startFile = initialFile;
        }

    protected:
        // Деструктор: освобождаем «родной» (native) объект результата вручную,
        // потому что сборщик мусора .NET о нём ничего не знает
        ~MainForm()
        {
            delete lastResult;
            lastResult = nullptr;
            if (components)
            {
                delete components;
            }
        }

    private:
        AnalysisResult* lastResult;   // результат последнего анализа (обычный указатель C++)
        bool highlightActive;         // в редакторе сейчас есть жёлтая подсветка
        bool updatingGrid;            // таблица перезаполняется — не реагировать на выбор ячеек
        String^ startFile;

        // ---------- элементы интерфейса ----------
        System::Windows::Forms::Panel^ panelHeader;
        System::Windows::Forms::Label^ lblTitle;
        System::Windows::Forms::Label^ lblSubtitle;
        System::Windows::Forms::Button^ btnOpen;
        System::Windows::Forms::Button^ btnAnalyze;
        System::Windows::Forms::Button^ btnCopy;
        System::Windows::Forms::CheckBox^ chkAuto;
        System::Windows::Forms::SplitContainer^ splitMain;
        System::Windows::Forms::Panel^ panelCodeCard;
        System::Windows::Forms::RichTextBox^ txtCode;
        System::Windows::Forms::Label^ lblFile;
        System::Windows::Forms::Label^ lblCodeTitle;
        System::Windows::Forms::TableLayoutPanel^ tableRight;
        System::Windows::Forms::Panel^ panelTableHead;
        System::Windows::Forms::Label^ lblTableTitle;
        System::Windows::Forms::Label^ lblBasic;
        System::Windows::Forms::DataGridView^ grid;
        System::Windows::Forms::DataGridViewTextBoxColumn^ colJ;
        System::Windows::Forms::DataGridViewTextBoxColumn^ colOp;
        System::Windows::Forms::DataGridViewTextBoxColumn^ colF1;
        System::Windows::Forms::DataGridViewTextBoxColumn^ colI;
        System::Windows::Forms::DataGridViewTextBoxColumn^ colOpd;
        System::Windows::Forms::DataGridViewTextBoxColumn^ colF2;
        System::Windows::Forms::TableLayoutPanel^ tableCards;
        System::Windows::Forms::Panel^ cardEta;
        System::Windows::Forms::Label^ lblEtaTitle;
        System::Windows::Forms::Label^ lblEtaValue;
        System::Windows::Forms::Label^ lblEtaFormula;
        System::Windows::Forms::Panel^ cardN;
        System::Windows::Forms::Label^ lblNTitle;
        System::Windows::Forms::Label^ lblNValue;
        System::Windows::Forms::Label^ lblNFormula;
        System::Windows::Forms::Panel^ cardV;
        System::Windows::Forms::Label^ lblVTitle;
        System::Windows::Forms::Label^ lblVValue;
        System::Windows::Forms::StatusStrip^ statusStrip;
        System::Windows::Forms::ToolStripStatusLabel^ lblStatus;
        System::Windows::Forms::Timer^ timer;
        System::Windows::Forms::OpenFileDialog^ openFileDialog;
        System::ComponentModel::Container^ components;

#pragma region Windows Form Designer generated code
        /// <summary>
        /// Создание всех элементов окна и настройка их свойств.
        /// Этот метод обычно генерирует конструктор форм Visual Studio.
        /// </summary>
        void InitializeComponent(void)
        {
            this->components = (gcnew System::ComponentModel::Container());
            this->panelHeader = (gcnew System::Windows::Forms::Panel());
            this->lblTitle = (gcnew System::Windows::Forms::Label());
            this->lblSubtitle = (gcnew System::Windows::Forms::Label());
            this->btnOpen = (gcnew System::Windows::Forms::Button());
            this->btnAnalyze = (gcnew System::Windows::Forms::Button());
            this->btnCopy = (gcnew System::Windows::Forms::Button());
            this->chkAuto = (gcnew System::Windows::Forms::CheckBox());
            this->splitMain = (gcnew System::Windows::Forms::SplitContainer());
            this->panelCodeCard = (gcnew System::Windows::Forms::Panel());
            this->txtCode = (gcnew System::Windows::Forms::RichTextBox());
            this->lblFile = (gcnew System::Windows::Forms::Label());
            this->lblCodeTitle = (gcnew System::Windows::Forms::Label());
            this->tableRight = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->panelTableHead = (gcnew System::Windows::Forms::Panel());
            this->lblTableTitle = (gcnew System::Windows::Forms::Label());
            this->lblBasic = (gcnew System::Windows::Forms::Label());
            this->grid = (gcnew System::Windows::Forms::DataGridView());
            this->colJ = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colOp = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colF1 = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colI = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colOpd = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colF2 = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->tableCards = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->cardEta = (gcnew System::Windows::Forms::Panel());
            this->lblEtaTitle = (gcnew System::Windows::Forms::Label());
            this->lblEtaValue = (gcnew System::Windows::Forms::Label());
            this->lblEtaFormula = (gcnew System::Windows::Forms::Label());
            this->cardN = (gcnew System::Windows::Forms::Panel());
            this->lblNTitle = (gcnew System::Windows::Forms::Label());
            this->lblNValue = (gcnew System::Windows::Forms::Label());
            this->lblNFormula = (gcnew System::Windows::Forms::Label());
            this->cardV = (gcnew System::Windows::Forms::Panel());
            this->lblVTitle = (gcnew System::Windows::Forms::Label());
            this->lblVValue = (gcnew System::Windows::Forms::Label());
            this->statusStrip = (gcnew System::Windows::Forms::StatusStrip());
            this->lblStatus = (gcnew System::Windows::Forms::ToolStripStatusLabel());
            this->timer = (gcnew System::Windows::Forms::Timer(this->components));
            this->openFileDialog = (gcnew System::Windows::Forms::OpenFileDialog());
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->splitMain))->BeginInit();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->grid))->BeginInit();
            this->panelHeader->SuspendLayout();
            this->splitMain->Panel1->SuspendLayout();
            this->splitMain->Panel2->SuspendLayout();
            this->splitMain->SuspendLayout();
            this->panelCodeCard->SuspendLayout();
            this->tableRight->SuspendLayout();
            this->panelTableHead->SuspendLayout();
            this->tableCards->SuspendLayout();
            this->cardEta->SuspendLayout();
            this->cardN->SuspendLayout();
            this->cardV->SuspendLayout();
            this->statusStrip->SuspendLayout();
            this->SuspendLayout();
            //
            // panelHeader — белая шапка с названием и кнопками
            //
            this->panelHeader->BackColor = System::Drawing::Color::White;
            this->panelHeader->Controls->Add(this->chkAuto);
            this->panelHeader->Controls->Add(this->btnCopy);
            this->panelHeader->Controls->Add(this->btnAnalyze);
            this->panelHeader->Controls->Add(this->btnOpen);
            this->panelHeader->Controls->Add(this->lblSubtitle);
            this->panelHeader->Controls->Add(this->lblTitle);
            this->panelHeader->Dock = System::Windows::Forms::DockStyle::Top;
            this->panelHeader->Location = System::Drawing::Point(0, 0);
            this->panelHeader->Name = L"panelHeader";
            this->panelHeader->Size = System::Drawing::Size(1380, 72);
            this->panelHeader->TabIndex = 0;
            //
            // lblTitle
            //
            this->lblTitle->AutoSize = true;
            this->lblTitle->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 14, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->lblTitle->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->lblTitle->Location = System::Drawing::Point(20, 12);
            this->lblTitle->Name = L"lblTitle";
            this->lblTitle->Text = L"Метрики Джилба и Маккейба";
            //
            // lblSubtitle
            //
            this->lblSubtitle->AutoSize = true;
            this->lblSubtitle->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblSubtitle->ForeColor = System::Drawing::Color::FromArgb(107, 114, 128);
            this->lblSubtitle->Location = System::Drawing::Point(22, 44);
            this->lblSubtitle->Name = L"lblSubtitle";
            this->lblSubtitle->Text = L"C++-парсер потока управления: анализируемый язык — Perl";
            //
            // chkAuto — пересчитывать при каждом изменении текста
            //
            this->chkAuto->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right);
            this->chkAuto->AutoSize = true;
            this->chkAuto->Checked = true;
            this->chkAuto->CheckState = System::Windows::Forms::CheckState::Checked;
            this->chkAuto->ForeColor = System::Drawing::Color::FromArgb(55, 65, 81);
            this->chkAuto->Location = System::Drawing::Point(700, 26);
            this->chkAuto->Name = L"chkAuto";
            this->chkAuto->Size = System::Drawing::Size(160, 24);
            this->chkAuto->Text = L"Пересчитывать при вводе";
            this->chkAuto->UseVisualStyleBackColor = true;
            //
            // btnOpen
            //
            this->btnOpen->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right);
            this->btnOpen->BackColor = System::Drawing::Color::FromArgb(243, 244, 246);
            this->btnOpen->FlatAppearance->BorderColor = System::Drawing::Color::FromArgb(209, 213, 219);
            this->btnOpen->FlatStyle = System::Windows::Forms::FlatStyle::Flat;
            this->btnOpen->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->btnOpen->Location = System::Drawing::Point(880, 18);
            this->btnOpen->Name = L"btnOpen";
            this->btnOpen->Size = System::Drawing::Size(150, 38);
            this->btnOpen->TabIndex = 1;
            this->btnOpen->Text = L"Открыть файл…";
            this->btnOpen->UseVisualStyleBackColor = false;
            this->btnOpen->Click += gcnew System::EventHandler(this, &MainForm::btnOpen_Click);
            //
            // btnAnalyze — главная кнопка (синяя)
            //
            this->btnAnalyze->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right);
            this->btnAnalyze->BackColor = System::Drawing::Color::FromArgb(37, 99, 235);
            this->btnAnalyze->FlatAppearance->BorderSize = 0;
            this->btnAnalyze->FlatStyle = System::Windows::Forms::FlatStyle::Flat;
            this->btnAnalyze->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 10, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->btnAnalyze->ForeColor = System::Drawing::Color::White;
            this->btnAnalyze->Location = System::Drawing::Point(1040, 18);
            this->btnAnalyze->Name = L"btnAnalyze";
            this->btnAnalyze->Size = System::Drawing::Size(140, 38);
            this->btnAnalyze->TabIndex = 2;
            this->btnAnalyze->Text = L"Рассчитать";
            this->btnAnalyze->UseVisualStyleBackColor = false;
            this->btnAnalyze->Click += gcnew System::EventHandler(this, &MainForm::btnAnalyze_Click);
            //
            // btnCopy — скопировать таблицу для отчёта
            //
            this->btnCopy->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right);
            this->btnCopy->BackColor = System::Drawing::Color::FromArgb(243, 244, 246);
            this->btnCopy->FlatAppearance->BorderColor = System::Drawing::Color::FromArgb(209, 213, 219);
            this->btnCopy->FlatStyle = System::Windows::Forms::FlatStyle::Flat;
            this->btnCopy->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->btnCopy->Location = System::Drawing::Point(1190, 18);
            this->btnCopy->Name = L"btnCopy";
            this->btnCopy->Size = System::Drawing::Size(170, 38);
            this->btnCopy->TabIndex = 3;
            this->btnCopy->Text = L"Копировать таблицу";
            this->btnCopy->UseVisualStyleBackColor = false;
            this->btnCopy->Click += gcnew System::EventHandler(this, &MainForm::btnCopy_Click);
            //
            // splitMain — слева код, справа результаты
            //
            this->splitMain->BackColor = System::Drawing::Color::FromArgb(243, 244, 246);
            this->splitMain->Dock = System::Windows::Forms::DockStyle::Fill;
            this->splitMain->Location = System::Drawing::Point(0, 72);
            this->splitMain->Name = L"splitMain";
            this->splitMain->Orientation = System::Windows::Forms::Orientation::Vertical;
            this->splitMain->Panel1->Controls->Add(this->panelCodeCard);
            this->splitMain->Panel1->Padding = System::Windows::Forms::Padding(20, 14, 0, 14);
            this->splitMain->Panel1MinSize = 320;
            this->splitMain->Panel2->Controls->Add(this->tableRight);
            this->splitMain->Panel2->Padding = System::Windows::Forms::Padding(0, 14, 20, 14);
            this->splitMain->Panel2MinSize = 520;
            this->splitMain->Size = System::Drawing::Size(1380, 766);
            this->splitMain->SplitterDistance = 600;
            this->splitMain->SplitterWidth = 12;
            this->splitMain->TabIndex = 1;
            //
            // panelCodeCard — белая «карточка» с редактором кода
            //
            this->panelCodeCard->BackColor = System::Drawing::Color::White;
            this->panelCodeCard->Controls->Add(this->txtCode);
            this->panelCodeCard->Controls->Add(this->lblFile);
            this->panelCodeCard->Controls->Add(this->lblCodeTitle);
            this->panelCodeCard->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelCodeCard->Name = L"panelCodeCard";
            this->panelCodeCard->Padding = System::Windows::Forms::Padding(14, 10, 14, 14);
            //
            // lblCodeTitle
            //
            this->lblCodeTitle->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblCodeTitle->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 11, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->lblCodeTitle->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->lblCodeTitle->Height = 28;
            this->lblCodeTitle->Name = L"lblCodeTitle";
            this->lblCodeTitle->Text = L"Исходный код анализируемой программы (Perl)";
            this->lblCodeTitle->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
            //
            // lblFile — путь к открытому файлу
            //
            this->lblFile->AutoEllipsis = true;
            this->lblFile->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblFile->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblFile->ForeColor = System::Drawing::Color::FromArgb(107, 114, 128);
            this->lblFile->Height = 24;
            this->lblFile->Name = L"lblFile";
            this->lblFile->Text = L"Файл не открыт — вставьте код или перетащите файл .pl в окно";
            this->lblFile->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
            //
            // txtCode — редактор исходного кода
            //
            this->txtCode->AcceptsTab = true;
            this->txtCode->AllowDrop = true;
            this->txtCode->BackColor = System::Drawing::Color::White;
            this->txtCode->BorderStyle = System::Windows::Forms::BorderStyle::None;
            this->txtCode->DetectUrls = false;
            this->txtCode->Dock = System::Windows::Forms::DockStyle::Fill;
            this->txtCode->Font = (gcnew System::Drawing::Font(L"Consolas", 10.5F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->txtCode->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->txtCode->HideSelection = false;
            this->txtCode->Name = L"txtCode";
            this->txtCode->ScrollBars = System::Windows::Forms::RichTextBoxScrollBars::Both;
            this->txtCode->WordWrap = false;
            this->txtCode->TextChanged += gcnew System::EventHandler(this, &MainForm::txtCode_TextChanged);
            this->txtCode->DragEnter += gcnew System::Windows::Forms::DragEventHandler(this, &MainForm::MainForm_DragEnter);
            this->txtCode->DragDrop += gcnew System::Windows::Forms::DragEventHandler(this, &MainForm::MainForm_DragDrop);
            //
            // tableRight — три ряда: заголовок таблицы, таблица, карточки метрик
            //
            this->tableRight->ColumnCount = 1;
            this->tableRight->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->tableRight->Controls->Add(this->panelTableHead, 0, 0);
            this->tableRight->Controls->Add(this->grid, 0, 1);
            this->tableRight->Controls->Add(this->tableCards, 0, 2);
            this->tableRight->Dock = System::Windows::Forms::DockStyle::Fill;
            this->tableRight->Margin = System::Windows::Forms::Padding(0);
            this->tableRight->Name = L"tableRight";
            this->tableRight->RowCount = 3;
            this->tableRight->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 36)));
            this->tableRight->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->tableRight->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 150)));
            //
            // panelTableHead
            //
            this->panelTableHead->Controls->Add(this->lblBasic);
            this->panelTableHead->Controls->Add(this->lblTableTitle);
            this->panelTableHead->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelTableHead->Margin = System::Windows::Forms::Padding(0);
            this->panelTableHead->Name = L"panelTableHead";
            //
            // lblTableTitle
            //
            this->lblTableTitle->AutoSize = true;
            this->lblTableTitle->Dock = System::Windows::Forms::DockStyle::Left;
            this->lblTableTitle->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 11, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->lblTableTitle->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->lblTableTitle->Name = L"lblTableTitle";
            this->lblTableTitle->Text = L"Операторы ветвления и операторы программы";
            this->lblTableTitle->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
            //
            // lblBasic — сводка η1, N1, η2, N2
            //
            this->lblBasic->AutoSize = true;
            this->lblBasic->Dock = System::Windows::Forms::DockStyle::Right;
            this->lblBasic->Font = (gcnew System::Drawing::Font(L"Segoe UI", 10, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblBasic->ForeColor = System::Drawing::Color::FromArgb(107, 114, 128);
            this->lblBasic->Name = L"lblBasic";
            this->lblBasic->Text = L"";
            this->lblBasic->TextAlign = System::Drawing::ContentAlignment::MiddleRight;
            //
            // grid — таблица операторов и операндов (как таблица 2 в методичке)
            //
            this->grid->AllowUserToAddRows = false;
            this->grid->AllowUserToDeleteRows = false;
            this->grid->AllowUserToResizeRows = false;
            this->grid->AlternatingRowsDefaultCellStyle->BackColor = System::Drawing::Color::FromArgb(249, 250, 251);
            this->grid->AutoSizeColumnsMode = System::Windows::Forms::DataGridViewAutoSizeColumnsMode::Fill;
            this->grid->BackgroundColor = System::Drawing::Color::White;
            this->grid->BorderStyle = System::Windows::Forms::BorderStyle::None;
            this->grid->CellBorderStyle = System::Windows::Forms::DataGridViewCellBorderStyle::SingleHorizontal;
            this->grid->ColumnHeadersBorderStyle = System::Windows::Forms::DataGridViewHeaderBorderStyle::None;
            this->grid->ColumnHeadersDefaultCellStyle->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
            this->grid->ColumnHeadersDefaultCellStyle->BackColor = System::Drawing::Color::FromArgb(229, 231, 235);
            this->grid->ColumnHeadersDefaultCellStyle->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 10, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->grid->ColumnHeadersDefaultCellStyle->ForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->grid->ColumnHeadersDefaultCellStyle->SelectionBackColor = System::Drawing::Color::FromArgb(229, 231, 235);
            this->grid->ColumnHeadersDefaultCellStyle->SelectionForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->grid->ColumnHeadersHeight = 34;
            this->grid->ColumnHeadersHeightSizeMode = System::Windows::Forms::DataGridViewColumnHeadersHeightSizeMode::DisableResizing;
            this->grid->Columns->AddRange(gcnew cli::array<System::Windows::Forms::DataGridViewColumn^>(6) { this->colJ, this->colOp, this->colF1, this->colI, this->colOpd, this->colF2 });
            this->grid->DefaultCellStyle->Padding = System::Windows::Forms::Padding(6, 0, 6, 0);
            this->grid->DefaultCellStyle->SelectionBackColor = System::Drawing::Color::FromArgb(219, 234, 254);
            this->grid->DefaultCellStyle->SelectionForeColor = System::Drawing::Color::FromArgb(17, 24, 39);
            this->grid->Dock = System::Windows::Forms::DockStyle::Fill;
            this->grid->EnableHeadersVisualStyles = false;
            this->grid->GridColor = System::Drawing::Color::FromArgb(229, 231, 235);
            this->grid->Margin = System::Windows::Forms::Padding(0);
            this->grid->MultiSelect = false;
            this->grid->Name = L"grid";
            this->grid->ReadOnly = true;
            this->grid->RowHeadersVisible = false;
            this->grid->RowTemplate->Height = 28;
            this->grid->SelectionMode = System::Windows::Forms::DataGridViewSelectionMode::CellSelect;
            this->grid->CellClick += gcnew System::Windows::Forms::DataGridViewCellEventHandler(this, &MainForm::grid_CellClick);
            this->grid->CurrentCellChanged += gcnew System::EventHandler(this, &MainForm::grid_CurrentCellChanged);
            //
            // столбцы: j | Оператор | f1j | i | Операнд | f2i
            //
            this->colJ->DefaultCellStyle->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
            this->colJ->FillWeight = 9;
            this->colJ->HeaderText = L"№";
            this->colJ->Name = L"colJ";
            this->colJ->ReadOnly = true;
            this->colJ->SortMode = System::Windows::Forms::DataGridViewColumnSortMode::NotSortable;
            this->colOp->FillWeight = 32;
            this->colOp->HeaderText = L"Ветвление";
            this->colOp->Name = L"colOp";
            this->colOp->ReadOnly = true;
            this->colOp->SortMode = System::Windows::Forms::DataGridViewColumnSortMode::NotSortable;
            this->colF1->DefaultCellStyle->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
            this->colF1->FillWeight = 10;
            this->colF1->HeaderText = L"Кол-во";
            this->colF1->Name = L"colF1";
            this->colF1->ReadOnly = true;
            this->colF1->SortMode = System::Windows::Forms::DataGridViewColumnSortMode::NotSortable;
            this->colI->DefaultCellStyle->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
            this->colI->FillWeight = 9;
            this->colI->HeaderText = L"№";
            this->colI->Name = L"colI";
            this->colI->ReadOnly = true;
            this->colI->SortMode = System::Windows::Forms::DataGridViewColumnSortMode::NotSortable;
            this->colOpd->FillWeight = 36;
            this->colOpd->HeaderText = L"Оператор";
            this->colOpd->Name = L"colOpd";
            this->colOpd->ReadOnly = true;
            this->colOpd->SortMode = System::Windows::Forms::DataGridViewColumnSortMode::NotSortable;
            this->colF2->DefaultCellStyle->Alignment = System::Windows::Forms::DataGridViewContentAlignment::MiddleCenter;
            this->colF2->FillWeight = 10;
            this->colF2->HeaderText = L"Кол-во";
            this->colF2->Name = L"colF2";
            this->colF2->ReadOnly = true;
            this->colF2->SortMode = System::Windows::Forms::DataGridViewColumnSortMode::NotSortable;
            //
            // tableCards — три карточки расширенных метрик
            //
            this->tableCards->ColumnCount = 3;
            this->tableCards->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 33.33F)));
            this->tableCards->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 33.33F)));
            this->tableCards->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 33.34F)));
            this->tableCards->Controls->Add(this->cardEta, 0, 0);
            this->tableCards->Controls->Add(this->cardN, 1, 0);
            this->tableCards->Controls->Add(this->cardV, 2, 0);
            this->tableCards->Dock = System::Windows::Forms::DockStyle::Fill;
            this->tableCards->Margin = System::Windows::Forms::Padding(0);
            this->tableCards->Name = L"tableCards";
            this->tableCards->RowCount = 1;
            this->tableCards->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
            //
            // cardEta — словарь программы
            //
            this->cardEta->BackColor = System::Drawing::Color::White;
            this->cardEta->Controls->Add(this->lblEtaFormula);
            this->cardEta->Controls->Add(this->lblEtaValue);
            this->cardEta->Controls->Add(this->lblEtaTitle);
            this->cardEta->Dock = System::Windows::Forms::DockStyle::Fill;
            this->cardEta->Margin = System::Windows::Forms::Padding(0, 14, 12, 0);
            this->cardEta->Name = L"cardEta";
            this->cardEta->Padding = System::Windows::Forms::Padding(16, 12, 16, 8);
            this->lblEtaTitle->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblEtaTitle->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblEtaTitle->ForeColor = System::Drawing::Color::FromArgb(107, 114, 128);
            this->lblEtaTitle->Height = 22;
            this->lblEtaTitle->Name = L"lblEtaTitle";
            this->lblEtaTitle->Text = L"Абсолютная сложность Джилба  CL";
            this->lblEtaValue->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblEtaValue->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 24, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->lblEtaValue->ForeColor = System::Drawing::Color::FromArgb(37, 99, 235);
            this->lblEtaValue->Height = 52;
            this->lblEtaValue->Name = L"lblEtaValue";
            this->lblEtaValue->Text = L"—";
            this->lblEtaFormula->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblEtaFormula->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9.5F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblEtaFormula->ForeColor = System::Drawing::Color::FromArgb(55, 65, 81);
            this->lblEtaFormula->Height = 44;
            this->lblEtaFormula->Name = L"lblEtaFormula";
            this->lblEtaFormula->Text = L"";
            //
            // cardN — длина программы
            //
            this->cardN->BackColor = System::Drawing::Color::White;
            this->cardN->Controls->Add(this->lblNFormula);
            this->cardN->Controls->Add(this->lblNValue);
            this->cardN->Controls->Add(this->lblNTitle);
            this->cardN->Dock = System::Windows::Forms::DockStyle::Fill;
            this->cardN->Margin = System::Windows::Forms::Padding(0, 14, 12, 0);
            this->cardN->Name = L"cardN";
            this->cardN->Padding = System::Windows::Forms::Padding(16, 12, 16, 8);
            this->lblNTitle->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblNTitle->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblNTitle->ForeColor = System::Drawing::Color::FromArgb(107, 114, 128);
            this->lblNTitle->Height = 22;
            this->lblNTitle->Name = L"lblNTitle";
            this->lblNTitle->Text = L"Относительная сложность  cl = CL / N";
            this->lblNValue->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblNValue->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 24, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->lblNValue->ForeColor = System::Drawing::Color::FromArgb(37, 99, 235);
            this->lblNValue->Height = 52;
            this->lblNValue->Name = L"lblNValue";
            this->lblNValue->Text = L"—";
            this->lblNFormula->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblNFormula->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9.5F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblNFormula->ForeColor = System::Drawing::Color::FromArgb(55, 65, 81);
            this->lblNFormula->Height = 44;
            this->lblNFormula->Name = L"lblNFormula";
            this->lblNFormula->Text = L"";
            //
            // cardV — объём программы
            //
            this->cardV->BackColor = System::Drawing::Color::White;
            this->cardV->Controls->Add(this->lblVValue);
            this->cardV->Controls->Add(this->lblVTitle);
            this->cardV->Dock = System::Windows::Forms::DockStyle::Fill;
            this->cardV->Margin = System::Windows::Forms::Padding(0, 14, 0, 0);
            this->cardV->Name = L"cardV";
            this->cardV->Padding = System::Windows::Forms::Padding(16, 12, 16, 8);
            this->lblVTitle->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblVTitle->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->lblVTitle->ForeColor = System::Drawing::Color::FromArgb(107, 114, 128);
            this->lblVTitle->Height = 22;
            this->lblVTitle->Name = L"lblVTitle";
            this->lblVTitle->Text = L"Оценка Маккейба V(G) / вложенность CLI";
            this->lblVValue->Dock = System::Windows::Forms::DockStyle::Top;
            this->lblVValue->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 24, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point));
            this->lblVValue->ForeColor = System::Drawing::Color::FromArgb(37, 99, 235);
            this->lblVValue->Height = 52;
            this->lblVValue->Name = L"lblVValue";
            this->lblVValue->Text = L"—";
            //
            // statusStrip
            //
            this->statusStrip->BackColor = System::Drawing::Color::White;
            this->statusStrip->Items->AddRange(gcnew cli::array<System::Windows::Forms::ToolStripItem^>(1) { this->lblStatus });
            this->statusStrip->Name = L"statusStrip";
            this->statusStrip->SizingGrip = false;
            this->lblStatus->ForeColor = System::Drawing::Color::FromArgb(55, 65, 81);
            this->lblStatus->Name = L"lblStatus";
            this->lblStatus->Text = L"Откройте файл с программой на Perl или вставьте её текст в редактор";
            //
            // timer — задержка перед пересчётом, чтобы не считать на каждую букву
            //
            this->timer->Interval = 350;
            this->timer->Tick += gcnew System::EventHandler(this, &MainForm::timer_Tick);
            //
            // openFileDialog
            //
            this->openFileDialog->Filter = L"Программы на Perl (*.pl;*.pm;*.t)|*.pl;*.pm;*.t|Все файлы (*.*)|*.*";
            this->openFileDialog->Title = L"Выберите программу на Perl";
            //
            // MainForm
            //
            this->AllowDrop = true;
            this->AutoScaleDimensions = System::Drawing::SizeF(96, 96);
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Dpi;
            this->BackColor = System::Drawing::Color::FromArgb(243, 244, 246);
            this->ClientSize = System::Drawing::Size(1380, 860);
            this->Controls->Add(this->splitMain);
            this->Controls->Add(this->statusStrip);
            this->Controls->Add(this->panelHeader);
            this->Font = (gcnew System::Drawing::Font(L"Segoe UI", 10, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point));
            this->MinimumSize = System::Drawing::Size(1100, 700);
            this->Name = L"MainForm";
            this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
            this->Text = L"Метрики Джилба и Маккейба — Perl";
            this->Load += gcnew System::EventHandler(this, &MainForm::MainForm_Load);
            this->DragEnter += gcnew System::Windows::Forms::DragEventHandler(this, &MainForm::MainForm_DragEnter);
            this->DragDrop += gcnew System::Windows::Forms::DragEventHandler(this, &MainForm::MainForm_DragDrop);
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->splitMain))->EndInit();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->grid))->EndInit();
            this->panelHeader->ResumeLayout(false);
            this->panelHeader->PerformLayout();
            this->splitMain->Panel1->ResumeLayout(false);
            this->splitMain->Panel2->ResumeLayout(false);
            this->splitMain->ResumeLayout(false);
            this->panelCodeCard->ResumeLayout(false);
            this->tableRight->ResumeLayout(false);
            this->panelTableHead->ResumeLayout(false);
            this->panelTableHead->PerformLayout();
            this->tableCards->ResumeLayout(false);
            this->cardEta->ResumeLayout(false);
            this->cardN->ResumeLayout(false);
            this->cardV->ResumeLayout(false);
            this->statusStrip->ResumeLayout(false);
            this->statusStrip->PerformLayout();
            this->ResumeLayout(false);
            this->PerformLayout();
        }
#pragma endregion

        // ====================================================================
        //  Обработчики событий
        // ====================================================================

        // Окно показано: подстроить высоту строк таблицы под масштаб экрана
        // и открыть файл, переданный в командной строке
        void MainForm_Load(Object^ sender, EventArgs^ e)
        {
            float scale = this->DeviceDpi / 96.0f;          // 1.0 при 100 %, 1.5 при 150 %
            grid->RowTemplate->Height = static_cast<int>(28 * scale);
            grid->ColumnHeadersHeight = static_cast<int>(34 * scale);
            if (startFile != nullptr && File::Exists(startFile))
            {
                LoadFile(startFile);
            }
        }

        void btnOpen_Click(Object^ sender, EventArgs^ e)
        {
            if (openFileDialog->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
            {
                LoadFile(openFileDialog->FileName);
            }
        }

        void btnAnalyze_Click(Object^ sender, EventArgs^ e)
        {
            timer->Stop();
            RunAnalysis();
        }

        // Текст изменился: если включён автопересчёт, перезапускаем таймер.
        // Пока пользователь печатает, таймер всё время сбрасывается, и анализ
        // запустится только через 350 мс после последнего нажатия клавиши.
        void txtCode_TextChanged(Object^ sender, EventArgs^ e)
        {
            if (chkAuto->Checked)
            {
                timer->Stop();
                timer->Start();
            }
        }

        void timer_Tick(Object^ sender, EventArgs^ e)
        {
            timer->Stop();
            RunAnalysis();
        }

        void grid_CellClick(Object^ sender, DataGridViewCellEventArgs^ e)
        {
            HighlightFromGrid(e->RowIndex, e->ColumnIndex);
        }

        void grid_CurrentCellChanged(Object^ sender, EventArgs^ e)
        {
            if (updatingGrid || grid->CurrentCell == nullptr) return;
            HighlightFromGrid(grid->CurrentCell->RowIndex, grid->CurrentCell->ColumnIndex);
        }

        // Перетаскивание файла в окно
        void MainForm_DragEnter(Object^ sender, DragEventArgs^ e)
        {
            if (e->Data->GetDataPresent(DataFormats::FileDrop)) e->Effect = DragDropEffects::Copy;
        }

        void MainForm_DragDrop(Object^ sender, DragEventArgs^ e)
        {
            cli::array<String^>^ files = safe_cast<cli::array<String^>^>(e->Data->GetData(DataFormats::FileDrop));
            if (files != nullptr && files->Length > 0) LoadFile(files[0]);
        }

        // Скопировать таблицу и метрики в буфер обмена (удобно вставлять в отчёт)
        void btnCopy_Click(Object^ sender, EventArgs^ e)
        {
            if (lastResult == nullptr) return;
            System::Text::StringBuilder^ sb = gcnew System::Text::StringBuilder();
            sb->AppendLine(L"№\tВетвление\tКоличество\t№\tОператор программы\tКоличество");
            size_t rows = lastResult->operators.size();
            if (lastResult->operands.size() > rows) rows = lastResult->operands.size();
            for (size_t r = 0; r < rows; r++)
            {
                if (r < lastResult->operators.size())
                    sb->Append(String::Format(L"{0}\t{1}\t{2}\t", r + 1, ToNet(lastResult->operators[r].name), lastResult->operators[r].count));
                else
                    sb->Append(L"\t\t\t");
                if (r < lastResult->operands.size())
                    sb->Append(String::Format(L"{0}\t{1}\t{2}", r + 1, ToNet(lastResult->operands[r].name), lastResult->operands[r].count));
                sb->AppendLine();
            }
            sb->AppendLine(String::Format(L"CL = {0}\tN = {1}\tCLI = {2}\tV(G) = {3}", lastResult->eta1, lastResult->N1, lastResult->eta2, lastResult->N2));
            sb->AppendLine();
            sb->AppendLine(String::Format(L"Относительная сложность cl = CL / N = {0} / {1} = {2:F4}", lastResult->eta1, lastResult->N1, lastResult->V));
            sb->AppendLine(String::Format(L"Оценка цикломатической сложности V(G) = {0} (с учётом отдельных подпрограмм и кратких логических операций)", lastResult->N2));
            sb->AppendLine(String::Format(L"Максимальный уровень вложенности CLI = {0}", lastResult->eta2));
            Clipboard::SetText(sb->ToString());
            lblStatus->Text = L"Таблица и метрики скопированы в буфер обмена";
        }

        // ====================================================================
        //  Логика
        // ====================================================================

        void LoadFile(String^ path)
        {
            try
            {
                String^ text = File::ReadAllText(path);        // кодировка: UTF-8 (с BOM или без)
                lblFile->Text = path;
                this->Text = L"Метрики Джилба и Маккейба — " + Path::GetFileName(path);
                timer->Stop();
                txtCode->Text = text;                            // вызовет TextChanged, но таймер мы остановим
                timer->Stop();
                RunAnalysis();
            }
            catch (Exception^ ex)
            {
                MessageBox::Show(this, L"Не удалось открыть файл:\n" + ex->Message, L"Ошибка",
                                 MessageBoxButtons::OK, MessageBoxIcon::Error);
            }
        }

        // Главная операция: текст из редактора -> анализатор -> таблица и карточки
        void RunAnalysis()
        {
            if (highlightActive) ClearHighlight();

            std::wstring code = ToStd(txtCode->Text);
            PerlControlFlowAnalyzer analyzer;                         // обычный объект C++ на стеке
            AnalysisResult* fresh = new AnalysisResult(analyzer.analyze(code));
            delete lastResult;                                       // старый результат больше не нужен
            lastResult = fresh;

            FillGrid();
            FillCards();

            lblBasic->Text = String::Format(L"CL = {0}     N = {1}     CLI = {2}     V(G) = {3}",
                                            lastResult->eta1, lastResult->N1, lastResult->eta2, lastResult->N2);
            lblStatus->Text = String::Format(L"Строк: {0}   ·   операторов: {1}   ·   ветвлений: {2}   ·   cl = {3:F4}   ·   щёлкните по строке для подсветки",
                                             lastResult->lineCount, lastResult->N1, lastResult->eta1, lastResult->V);
        }

        // Заполнить таблицу: слева операторы, справа операнды, внизу итоги
        void FillGrid()
        {
            updatingGrid = true;
            grid->SuspendLayout();
            grid->Rows->Clear();

            const std::vector<Entry>& ops = lastResult->operators;
            const std::vector<Entry>& opds = lastResult->operands;
            size_t rows = ops.size() > opds.size() ? ops.size() : opds.size();
            for (size_t r = 0; r < rows; r++)
            {
                cli::array<Object^>^ cells = gcnew cli::array<Object^>(6);
                if (r < ops.size())
                {
                    cells[0] = (r + 1).ToString() + L".";
                    cells[1] = ToNet(ops[r].name);
                    cells[2] = ops[r].count;
                }
                if (r < opds.size())
                {
                    cells[3] = (r + 1).ToString() + L".";
                    cells[4] = ToNet(opds[r].name);
                    cells[5] = opds[r].count;
                }
                grid->Rows->Add(cells);
            }

            // Итоговая строка метрик потока управления
            int idx = grid->Rows->Add(gcnew cli::array<Object^>(6) {
                L"CL = " + lastResult->eta1, L"", L"N = " + lastResult->N1,
                L"CLI = " + lastResult->eta2, L"", L"V(G) = " + lastResult->N2 });
            DataGridViewRow^ total = grid->Rows[idx];
            total->DefaultCellStyle->BackColor = Color::FromArgb(238, 242, 255);
            total->DefaultCellStyle->SelectionBackColor = Color::FromArgb(238, 242, 255);
            total->DefaultCellStyle->Font = gcnew System::Drawing::Font(L"Segoe UI Semibold", 10, FontStyle::Bold, GraphicsUnit::Point);
            total->DefaultCellStyle->Alignment = DataGridViewContentAlignment::MiddleLeft;

            grid->ClearSelection();
            grid->ResumeLayout();
            updatingGrid = false;
        }

        // Заполнить карточки расширенных метрик
        void FillCards()
        {
            lblEtaValue->Text = lastResult->eta1.ToString();
            lblEtaFormula->Text = L"Сумма всех условий, циклов и ветвей when";

            lblNValue->Text = String::Format(L"{0:F4}", lastResult->V);
            lblNFormula->Text = String::Format(L"cl = {0} / {1}", lastResult->eta1, lastResult->N1);

            lblVValue->Text = String::Format(L"{0} / {1}", lastResult->N2, lastResult->eta2);
        }

        // Щелчок по строке таблицы: подсветить в коде все вхождения выбранного оператора/операнда
        void HighlightFromGrid(int row, int col)
        {
            if (lastResult == nullptr || row < 0 || col < 0) return;
            bool operatorSide = (col <= 2);                          // первые три столбца — операторы
            const std::vector<Entry>& table = operatorSide ? lastResult->operators : lastResult->operands;
            if (row >= static_cast<int>(table.size())) { ClearHighlight(); return; }

            const Entry& entry = table[row];
            BeginUpdate();
            int caret = txtCode->SelectionStart;
            txtCode->SelectAll();
            txtCode->SelectionBackColor = Color::White;              // убрать прошлую подсветку
            for (size_t k = 0; k < entry.tokenIndexes.size(); k++)
            {
                const Token& t = lastResult->tokens[entry.tokenIndexes[k]];
                txtCode->Select(t.start, t.length);
                txtCode->SelectionBackColor = Color::FromArgb(253, 230, 138);   // жёлтый маркер
            }
            if (!entry.tokenIndexes.empty())
            {
                const Token& first = lastResult->tokens[entry.tokenIndexes.front()];
                txtCode->Select(first.start, 0);
                txtCode->ScrollToCaret();                            // показать первое вхождение
            }
            else txtCode->Select(caret, 0);
            EndUpdate();
            highlightActive = true;

            lblStatus->Text = String::Format(L"{0} «{1}»: {2} вхождений подсвечено в коде",
                                             operatorSide ? L"Ветвление" : L"Оператор", ToNet(entry.name), entry.count);
        }

        void ClearHighlight()
        {
            BeginUpdate();
            int caret = txtCode->SelectionStart;
            txtCode->SelectAll();
            txtCode->SelectionBackColor = Color::White;
            txtCode->Select(caret, 0);
            EndUpdate();
            highlightActive = false;
        }

        // Запретить/разрешить перерисовку редактора, чтобы подсветка не мигала.
        // WM_SETREDRAW = 0x000B — сообщение Windows «перерисовывать или нет».
        void BeginUpdate()
        {
            Native::SendMessage(txtCode->Handle, 0x000B, IntPtr(0), IntPtr(0));
        }

        void EndUpdate()
        {
            Native::SendMessage(txtCode->Handle, 0x000B, IntPtr(1), IntPtr(0));
            txtCode->Invalidate();
        }
    };
}
