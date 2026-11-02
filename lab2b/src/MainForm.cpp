#include "PerlBoundaryAnalyzer.h"

#include <vcclr.h>

using namespace System;
using namespace System::Drawing;
using namespace System::Globalization;
using namespace System::IO;
using namespace System::Text;
using namespace System::Windows::Forms;

static std::wstring toNative(String^ value) {
    if (value == nullptr) return {};
    pin_ptr<const wchar_t> text = PtrToStringChars(value);
    return std::wstring(text, value->Length);
}

static String^ toManaged(const std::wstring& value) {
    return gcnew String(value.c_str(), 0, static_cast<int>(value.size()));
}

ref class MainForm : Form {
public:
    MainForm(String^ initialFile) {
        this->initialFile = initialFile;
        this->Text = L"Метрика граничных значений — Perl";
        this->Font = gcnew Drawing::Font(L"Segoe UI", 10);
        this->BackColor = Color::FromArgb(243, 244, 246);
        this->MinimumSize = Drawing::Size(1080, 660);
        this->Size = Drawing::Size(1400, 850);
        this->StartPosition = FormStartPosition::CenterScreen;

        auto root = gcnew TableLayoutPanel();
        root->Dock = DockStyle::Fill;
        root->ColumnCount = 1;
        root->RowCount = 3;
        root->RowStyles->Add(gcnew RowStyle(SizeType::Absolute, 76));
        root->RowStyles->Add(gcnew RowStyle(SizeType::Percent, 100));
        root->RowStyles->Add(gcnew RowStyle(SizeType::Absolute, 28));
        this->Controls->Add(root);

        auto header = gcnew Panel();
        header->Dock = DockStyle::Fill;
        header->BackColor = Color::White;
        root->Controls->Add(header, 0, 0);

        auto title = gcnew Label();
        title->AutoSize = true;
        title->Font = gcnew Drawing::Font(L"Segoe UI Semibold", 15, FontStyle::Bold);
        title->Text = L"Метрика граничных значений";
        title->Location = Point(20, 9);
        header->Controls->Add(title);

        auto subtitle = gcnew Label();
        subtitle->AutoSize = true;
        subtitle->Text = L"C++ анализирует текущий код Perl и пересчитывает Sa и So";
        subtitle->ForeColor = Color::FromArgb(100, 108, 120);
        subtitle->Location = Point(22, 45);
        header->Controls->Add(subtitle);

        auto buttonOpen = makeButton(L"Открыть файл", 980, 18, 130, false);
        auto buttonSave = makeButton(L"Сохранить", 1120, 18, 100, false);
        auto buttonAnalyze = makeButton(L"Рассчитать", 1230, 18, 130, true);
        buttonOpen->Anchor = AnchorStyles::Top | AnchorStyles::Right;
        buttonSave->Anchor = AnchorStyles::Top | AnchorStyles::Right;
        buttonAnalyze->Anchor = AnchorStyles::Top | AnchorStyles::Right;
        header->Controls->Add(buttonOpen);
        header->Controls->Add(buttonSave);
        header->Controls->Add(buttonAnalyze);
        buttonOpen->Click += gcnew EventHandler(this, &MainForm::openClick);
        buttonSave->Click += gcnew EventHandler(this, &MainForm::saveClick);
        buttonAnalyze->Click += gcnew EventHandler(this, &MainForm::analyzeClick);

        autoRecalculate = gcnew CheckBox();
        autoRecalculate->Text = L"Пересчитывать при вводе";
        autoRecalculate->Checked = true;
        autoRecalculate->AutoSize = true;
        autoRecalculate->Location = Point(760, 27);
        autoRecalculate->Anchor = AnchorStyles::Top | AnchorStyles::Right;
        header->Controls->Add(autoRecalculate);

        status = gcnew StatusStrip();
        status->Dock = DockStyle::Fill;
        statusText = gcnew ToolStripStatusLabel(L"Готово");
        status->Items->Add(statusText);
        root->Controls->Add(status, 0, 2);

        auto split = gcnew SplitContainer();
        split->Dock = DockStyle::Fill;
        split->Orientation = Orientation::Vertical;
        split->Size = Drawing::Size(1380, 730);
        split->Panel1MinSize = 340;
        split->Panel2MinSize = 540;
        split->SplitterWidth = 10;
        split->SplitterDistance = 570;
        root->Controls->Add(split, 0, 1);

        auto codePanel = gcnew Panel();
        codePanel->Dock = DockStyle::Fill;
        codePanel->BackColor = Color::White;
        codePanel->Padding = Windows::Forms::Padding(14);
        split->Panel1->Padding = Windows::Forms::Padding(16, 14, 6, 14);
        split->Panel1->Controls->Add(codePanel);

        auto codeLayout = gcnew TableLayoutPanel();
        codeLayout->Dock = DockStyle::Fill;
        codeLayout->ColumnCount = 1;
        codeLayout->RowCount = 2;
        codeLayout->RowStyles->Add(gcnew RowStyle(SizeType::Absolute, 30));
        codeLayout->RowStyles->Add(gcnew RowStyle(SizeType::Percent, 100));
        codePanel->Controls->Add(codeLayout);

        fileLabel = gcnew Label();
        fileLabel->Dock = DockStyle::Fill;
        fileLabel->Text = L"Код Perl";
        fileLabel->Font = gcnew Drawing::Font(L"Segoe UI Semibold", 10, FontStyle::Bold);
        codeLayout->Controls->Add(fileLabel, 0, 0);

        codeEditor = gcnew RichTextBox();
        codeEditor->Dock = DockStyle::Fill;
        codeEditor->Font = gcnew Drawing::Font(L"Consolas", 10);
        codeEditor->BorderStyle = BorderStyle::None;
        codeEditor->AcceptsTab = true;
        codeEditor->WordWrap = false;
        codeEditor->ScrollBars = RichTextBoxScrollBars::Both;
        codeEditor->TextChanged += gcnew EventHandler(this, &MainForm::codeChanged);
        codeLayout->Controls->Add(codeEditor, 0, 1);

        auto resultPanel = gcnew Panel();
        resultPanel->Dock = DockStyle::Fill;
        resultPanel->BackColor = Color::White;
        split->Panel2->Padding = Windows::Forms::Padding(6, 14, 16, 14);
        split->Panel2->Controls->Add(resultPanel);

        auto resultLayout = gcnew TableLayoutPanel();
        resultLayout->Dock = DockStyle::Fill;
        resultLayout->ColumnCount = 1;
        resultLayout->RowCount = 2;
        resultLayout->RowStyles->Add(gcnew RowStyle(SizeType::Absolute, 112));
        resultLayout->RowStyles->Add(gcnew RowStyle(SizeType::Percent, 100));
        resultPanel->Controls->Add(resultLayout);

        auto cards = gcnew TableLayoutPanel();
        cards->Dock = DockStyle::Fill;
        cards->ColumnCount = 3;
        cards->RowCount = 1;
        cards->ColumnStyles->Add(gcnew ColumnStyle(SizeType::Percent, 33.33f));
        cards->ColumnStyles->Add(gcnew ColumnStyle(SizeType::Percent, 33.33f));
        cards->ColumnStyles->Add(gcnew ColumnStyle(SizeType::Percent, 33.34f));
        resultLayout->Controls->Add(cards, 0, 0);
        valueV = addCard(cards, 0, L"Вершины v");
        valueSa = addCard(cards, 1, L"Абсолютная Sa");
        valueSo = addCard(cards, 2, L"Относительная So");

        auto tabs = gcnew TabControl();
        tabs->Dock = DockStyle::Fill;
        auto tabNodes = gcnew TabPage(L"Все вершины");
        auto tabChoices = gcnew TabPage(L"Вершины выбора");
        tabs->TabPages->Add(tabNodes);
        tabs->TabPages->Add(tabChoices);
        resultLayout->Controls->Add(tabs, 0, 1);
        nodesGrid = createGrid(tabNodes);
        nodesGrid->Columns->Add(makeColumn(L"Номер", 70));
        nodesGrid->Columns->Add(makeColumn(L"Тип", 95));
        nodesGrid->Columns->Add(makeColumn(L"Оператор или условие", 400));
        nodesGrid->Columns->Add(makeColumn(L"Сложность", 105));
        nodesGrid->Columns[2]->AutoSizeMode = DataGridViewAutoSizeColumnMode::Fill;
        choicesGrid = createGrid(tabChoices);
        choicesGrid->Columns->Add(makeColumn(L"Выбор", 70));
        choicesGrid->Columns->Add(makeColumn(L"Переходы", 130));
        choicesGrid->Columns->Add(makeColumn(L"Подграф", 350));
        choicesGrid->Columns->Add(makeColumn(L"Граница", 80));
        choicesGrid->Columns->Add(makeColumn(L"Сложность", 105));
        choicesGrid->Columns[2]->AutoSizeMode = DataGridViewAutoSizeColumnMode::Fill;

        recalcTimer = gcnew Windows::Forms::Timer();
        recalcTimer->Interval = 350;
        recalcTimer->Tick += gcnew EventHandler(this, &MainForm::timerTick);
        this->Shown += gcnew EventHandler(this, &MainForm::formShown);
    }

private:
    String^ initialFile;
    String^ currentFile;
    bool loading = false;
    CheckBox^ autoRecalculate;
    RichTextBox^ codeEditor;
    Label^ fileLabel;
    Label^ valueV;
    Label^ valueSa;
    Label^ valueSo;
    DataGridView^ nodesGrid;
    DataGridView^ choicesGrid;
    StatusStrip^ status;
    ToolStripStatusLabel^ statusText;
    Windows::Forms::Timer^ recalcTimer;

    Button^ makeButton(String^ caption, int x, int y, int width, bool primary) {
        auto button = gcnew Button();
        button->Text = caption;
        button->Location = Point(x, y);
        button->Size = Drawing::Size(width, 38);
        button->FlatStyle = FlatStyle::Flat;
        button->FlatAppearance->BorderSize = primary ? 0 : 1;
        button->BackColor = primary ? Color::FromArgb(37, 99, 235) : Color::FromArgb(243, 244, 246);
        button->ForeColor = primary ? Color::White : Color::FromArgb(17, 24, 39);
        return button;
    }
    Label^ addCard(TableLayoutPanel^ cards, int column, String^ caption) {
        auto panel = gcnew Panel();
        panel->Dock = DockStyle::Fill;
        panel->Margin = Windows::Forms::Padding(8);
        panel->BackColor = Color::FromArgb(247, 249, 252);
        auto layout = gcnew TableLayoutPanel();
        layout->Dock = DockStyle::Fill;
        layout->ColumnCount = 1;
        layout->RowCount = 2;
        layout->RowStyles->Add(gcnew RowStyle(SizeType::Absolute, 28));
        layout->RowStyles->Add(gcnew RowStyle(SizeType::Percent, 100));
        panel->Controls->Add(layout);
        auto title = gcnew Label();
        title->Text = caption;
        title->Dock = DockStyle::Fill;
        title->ForeColor = Color::FromArgb(75, 85, 99);
        title->Padding = Windows::Forms::Padding(10, 5, 0, 0);
        auto value = gcnew Label();
        value->Text = L"—";
        value->Dock = DockStyle::Fill;
        value->Font = gcnew Drawing::Font(L"Segoe UI Semibold", 20, FontStyle::Bold);
        value->ForeColor = Color::FromArgb(37, 99, 235);
        value->Padding = Windows::Forms::Padding(10, 0, 0, 0);
        layout->Controls->Add(title, 0, 0);
        layout->Controls->Add(value, 0, 1);
        cards->Controls->Add(panel, column, 0);
        return value;
    }
    DataGridView^ createGrid(TabPage^ tab) {
        auto grid = gcnew DataGridView();
        grid->Dock = DockStyle::Fill;
        grid->ReadOnly = true;
        grid->AllowUserToAddRows = false;
        grid->AllowUserToDeleteRows = false;
        grid->RowHeadersVisible = false;
        grid->SelectionMode = DataGridViewSelectionMode::FullRowSelect;
        grid->AutoSizeRowsMode = DataGridViewAutoSizeRowsMode::AllCells;
        grid->BackgroundColor = Color::White;
        grid->BorderStyle = BorderStyle::None;
        tab->Controls->Add(grid);
        return grid;
    }
    DataGridViewTextBoxColumn^ makeColumn(String^ heading, int width) {
        auto column = gcnew DataGridViewTextBoxColumn();
        column->HeaderText = heading;
        column->Width = width;
        column->SortMode = DataGridViewColumnSortMode::NotSortable;
        return column;
    }
    static String^ numbers(const std::vector<int>& values) {
        auto text = gcnew StringBuilder();
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) text->Append(L", ");
            text->Append(values[i]);
        }
        return text->ToString();
    }
    void loadFile(String^ path) {
        loading = true;
        try {
            codeEditor->Text = File::ReadAllText(path);
            currentFile = path;
            fileLabel->Text = L"Код Perl — " + Path::GetFileName(path);
            this->Text = L"Метрика граничных значений — " + Path::GetFileName(path);
        } finally { loading = false; }
        recalcTimer->Stop();
        analyzeCode();
    }
    void formShown(Object^, EventArgs^) {
        String^ path = initialFile;
        if (String::IsNullOrEmpty(path))
            path = Path::GetFullPath(Path::Combine(Application::StartupPath,
                                                  L"..\\..\\..\\perl\\boundary_sample.pl"));
        if (File::Exists(path)) {
            try { loadFile(path); }
            catch (Exception^ error) { statusText->Text = error->Message; }
        } else {
            statusText->Text = L"Откройте Perl-файл или вставьте код слева";
        }
    }
    void openClick(Object^, EventArgs^) {
        auto dialog = gcnew OpenFileDialog();
        dialog->Filter = L"Perl (*.pl;*.pm)|*.pl;*.pm|Все файлы (*.*)|*.*";
        if (dialog->ShowDialog(this) == System::Windows::Forms::DialogResult::OK) {
            try { loadFile(dialog->FileName); }
            catch (Exception^ error) { MessageBox::Show(this, error->Message, L"Ошибка открытия"); }
        }
    }
    void saveClick(Object^, EventArgs^) {
        String^ path = currentFile;
        if (String::IsNullOrEmpty(path)) {
            auto dialog = gcnew SaveFileDialog();
            dialog->Filter = L"Perl (*.pl)|*.pl|Все файлы (*.*)|*.*";
            if (dialog->ShowDialog(this) != System::Windows::Forms::DialogResult::OK) return;
            path = dialog->FileName;
        }
        try {
            File::WriteAllText(path, codeEditor->Text, gcnew UTF8Encoding(false));
            currentFile = path;
            fileLabel->Text = L"Код Perl — " + Path::GetFileName(path);
            statusText->Text = L"Сохранено: " + path;
        } catch (Exception^ error) {
            MessageBox::Show(this, error->Message, L"Ошибка сохранения");
        }
    }
    void codeChanged(Object^, EventArgs^) {
        if (loading || !autoRecalculate->Checked) return;
        recalcTimer->Stop();
        recalcTimer->Start();
    }
    void timerTick(Object^, EventArgs^) {
        recalcTimer->Stop();
        analyzeCode();
    }
    void analyzeClick(Object^, EventArgs^) {
        recalcTimer->Stop();
        analyzeCode();
    }
    void analyzeCode() {
        try {
            const BoundaryReport report = analyzePerlBoundary(toNative(codeEditor->Text));
            nodesGrid->Rows->Clear();
            choicesGrid->Rows->Clear();
            for (const auto& node : report.nodes) {
                nodesGrid->Rows->Add(gcnew cli::array<Object^>(4) {
                    node.id, toManaged(node.kind), toManaged(node.label), node.complexity});
            }
            for (const auto& choice : report.choices) {
                choicesGrid->Rows->Add(gcnew cli::array<Object^>(5) {
                    choice.id, numbers(choice.successors), numbers(choice.subgraph),
                    choice.boundary, choice.complexity});
            }
            valueV->Text = Convert::ToString(report.vertices);
            valueSa->Text = Convert::ToString(report.Sa);
            valueSo->Text = String::Format(CultureInfo::GetCultureInfo(L"ru-RU"),
                                            L"{0:F4}", report.So);
            statusText->ForeColor = Color::FromArgb(22, 101, 52);
            statusText->Text = String::Format(L"Расчёт выполнен по текущему тексту: {0} вершин выбора",
                                              static_cast<int>(report.choices.size()));
        } catch (const std::exception& error) {
            nodesGrid->Rows->Clear();
            choicesGrid->Rows->Clear();
            valueV->Text = L"—";
            valueSa->Text = L"—";
            valueSo->Text = L"—";
            statusText->ForeColor = Color::FromArgb(185, 28, 28);
            statusText->Text = gcnew String(error.what());
        }
    }
};

static int exportTable(String^ perlFile, String^ outputFile) {
    try {
        const BoundaryReport report = analyzePerlBoundary(toNative(File::ReadAllText(perlFile)));
        auto table = gcnew StringBuilder();
        table->AppendLine(String::Format(CultureInfo::InvariantCulture,
                                         L"METRICS\tv={0}\tSa={1}\tSo={2:F4}",
                                         report.vertices, report.Sa, report.So));
        table->AppendLine(L"ID\tKIND\tLABEL\tCOMPLEXITY");
        for (const auto& node : report.nodes)
            table->AppendLine(String::Format(L"{0}\t{1}\t{2}\t{3}",
                                             node.id, toManaged(node.kind),
                                             toManaged(node.label), node.complexity));
        File::WriteAllText(outputFile, table->ToString(), gcnew UTF8Encoding(false));
        return 0;
    } catch (const std::exception& error) {
        File::WriteAllText(outputFile, L"ERROR\t" + gcnew String(error.what()));
        return 1;
    } catch (Exception^ error) {
        File::WriteAllText(outputFile, L"ERROR\t" + error->Message);
        return 1;
    }
}

[STAThreadAttribute]
int main(cli::array<String^>^ args) {
    if (args->Length >= 3 && args[1] == L"--export")
        return exportTable(args[0], args[2]);
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    String^ path = args->Length ? args[0] : nullptr;
    MainForm form(path);
    Application::Run(%form);
    return 0;
}
