#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_set>
using namespace std;

struct Procedure
{
    int line;
    string name;
    vector<string> ins;
    vector<string> outs;

    Procedure(int line, string name, vector<string> ins, vector<string> outs)
        : line(line), name(name), ins(ins), outs(outs) {}
};

struct Chip
{
    string name;
    vector<string> ins;
    vector<string> outs;
    vector<Procedure> procedures;

    vector<bool> execute(const map<string, Chip> &chips, const vector<bool> &inputs, int debug) const;
};

#define THROW_SIGNAL_NOT_FOUND(signal)                           \
    {                                                            \
        cout << "Line " << p.line + 1 << " available signals: "; \
        for (const auto &pair : signals)                         \
            cout << pair.first << " ";                           \
        cout << endl;                                            \
        throw runtime_error("Input " + signal + " not found");   \
    }
#define RESOLVE_SIGNAL(signal, out)        \
    if (signal == "0" || signal == "1")    \
    {                                      \
        out = (signal == "1");             \
    }                                      \
    else                                   \
    {                                      \
        auto it = signals.find(signal);    \
        if (it == signals.end())           \
            THROW_SIGNAL_NOT_FOUND(signal) \
        out = it->second;                  \
    }
string repeat(int n, string s);
vector<vector<bool>> print;
void Flush();

vector<bool> Chip::execute(const map<string, Chip> &chips, const vector<bool> &inputs, int debug) const
{
    map<string, bool> signals;
    for (const auto &out : outs)
        signals[out] = false;
    for (size_t i = 0; i < ins.size(); ++i)
        signals[ins[i]] = inputs[i];
    if (debug)
    {
        cout << repeat(debug, "│") << "┌╴" << name << "  \033[32m";
        for (const auto &in : ins)
            cout << in << " " << signals[in] << "  ";
        cout << "\033[0m" << endl;
    }
    for (const auto &p : procedures)
    {
        if (p.name == "->")
        {
            if (p.ins.size() != p.outs.size())
                throw runtime_error("Line " + to_string(p.line + 1) + ": imbalanced");
            for (size_t j = 0; j < p.ins.size(); ++j)
            {
                RESOLVE_SIGNAL(p.ins[j], signals[p.outs[j]])
            }
        }
        else if (p.name == "AND" || p.name == "NAND" || p.name == "OR" || p.name == "NOR")
        {
            if (p.outs.size() != 1)
                throw runtime_error("Line " + to_string(p.line + 1) + ": invalid " + p.name);
            bool OR = false;
            bool AND = true;
            for (const auto &in : p.ins)
            {
                bool x;
                RESOLVE_SIGNAL(in, x)
                OR = OR || x;
                AND = AND && x;
            }
            if (p.name == "AND")
                signals[p.outs[0]] = AND;
            if (p.name == "NAND")
                signals[p.outs[0]] = !AND;
            if (p.name == "OR")
                signals[p.outs[0]] = OR;
            if (p.name == "NOR")
                signals[p.outs[0]] = !OR;
            if (debug)
            {
                cout << repeat(debug, "│") << "├╴" << p.name << " \033[32m";
                cout << p.ins[0] << " " << signals[p.ins[0]] << "  ";
                cout << p.ins[1] << " " << signals[p.ins[1]] << "  \033[34m";
                cout << p.outs[0] << " " << signals[p.outs[0]] << " \033[0m" << endl;
            }
        }
        else if (p.name == "PrintChar" || p.name == "Print8" || p.name == "PrintBits")
        {
            if (p.ins.size() != 8)
                throw runtime_error("Line " + to_string(p.line + 1) + ": invalid");
            uint8_t byte = 0;
            for (size_t j = 0; j < 8; ++j)
            {
                bool x;
                RESOLVE_SIGNAL(p.ins[j], x)
                byte |= (x << (7 - j));
                if (p.name == "PrintBits")
                    cout << (x ? '1' : '0');
            }
            if (p.name == "PrintChar")
                cout << "\033[32m" << static_cast<char>(byte) << "\033[0m";
            else if (p.name == "Print8")
                cout << "\033[34m" << to_string(byte) << "\033[0m";
        }
        else if (p.name == "Flush")
        {
            Flush();
        }
        else
        {
            vector<bool> chipInputs;
            for (const auto &in : p.ins)
            {
                bool x;
                RESOLVE_SIGNAL(in, x)
                chipInputs.push_back(x);
            }
            if (p.name == "Print")
            {
                print.push_back(chipInputs);
            }
            else
            {
                auto chip = chips.find(p.name);
                if (chip == chips.end())
                    throw runtime_error("Line " + to_string(p.line + 1) + ": chip " + p.name + " not found");
                vector<bool> chipOutputs = chip->second.execute(chips, chipInputs, debug + !!debug);
                for (size_t j = 0; j < p.outs.size(); ++j)
                    signals[p.outs[j]] = chipOutputs[j];
            }
        }
    }
    vector<bool> outputs(outs.size());
    for (size_t i = 0; i < outs.size(); ++i)
        if (signals.find(outs[i]) != signals.end())
            outputs[i] = signals[outs[i]];
    if (debug)
    {
        cout << repeat(debug, "│") << "└╴\033[34m";
        for (auto &s : signals)
            cout << s.first << " " << s.second << "  ";
        cout << "\033[0m" << endl;
    }
    return outputs;
}

void EchoFile(const map<string, Chip> &chips);

vector<string> readFileToVector(const string &filename)
{
    ifstream source;
    source.open(filename);
    vector<string> lines;
    string line;
    while (getline(source, line))
    {
        lines.push_back(line);
    }
    source.close();
    return lines;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        throw runtime_error("Usage: " + string(argv[0]) + " <filename> [--echo-file] [--debug]");
    }
    string filename(argv[1]);
    vector<string> lines = readFileToVector(filename);
    map<string, Chip> chips;

    bool newChip = true;
    bool readIns = false;
    bool readProc = false;
    bool readOuts = false;
    string chipName, procName;
    for (int l = 0; l < lines.size(); ++l)
    {
        string line = lines[l];
        if (line[0] == '#')
        {
            continue;
        }
        string nextLine = (l + 1 < lines.size()) ? lines[l + 1] : "";
        if (line == "")
        {
            newChip = true;
        }
        else if (newChip)
        {
            chipName = line;
            chips[chipName] = Chip{chipName, {}, {}, {}};
            newChip = false;
            readIns = true;
        }
        else if (readIns || readOuts)
        {
            stringstream ss(line);
            string chunk;
            while (getline(ss, chunk, ' '))
                if (chunk != "")
                    if (readIns)
                        chips[chipName].ins.push_back(chunk);
                    else if (readOuts)
                        chips[chipName].outs.push_back(chunk);
            if (nextLine[0] == ' ' || nextLine[0] == '\0')
            {
                readProc = readIns;
                readIns = false;
                readOuts = false;
            }
        }
        else if (readProc)
        {
            vector<string> procIns, procOuts;
            string thisProcName = "";
            bool readProcIns = false;
            bool readProcOuts = false;
            stringstream ss(line);
            string chunk;
            while (getline(ss, chunk, ' '))
            {
                if (chunk == "")
                {
                    if (procIns.size() && readProcIns)
                    {
                        readProcIns = false;
                        readProcOuts = true;
                    }
                }
                else if (thisProcName.empty())
                {
                    if (chunk != "\"")
                        procName = chunk;
                    thisProcName = procName;
                    readProcIns = true;
                }
                else if (readProcIns)
                {
                    procIns.push_back(chunk);
                }
                else if (readProcOuts)
                {
                    procOuts.push_back(chunk);
                }
            }
            chips[chipName].procedures.push_back({l, thisProcName, procIns, procOuts});
            if (nextLine[0] != ' ' && nextLine[0] != '#')
            {
                readProc = false;
                readOuts = true;
            }
        }
    }

    unordered_set<string> args(argv + 1, argv + argc);
    if (args.count("--echo-file"))
        EchoFile(chips);
    auto debug = args.count("--debug");
    if (debug)
        cout << "┌╴Main" << endl;

    chips["Main"].execute(chips, {false}, debug);

    return 0;
}

void EchoFile(const map<string, Chip> &chips)
{
    for (const auto &pair : chips)
    {
        cout << pair.first << '(' << pair.second.ins.size() << ')' << endl;
        for (const auto &in : pair.second.ins)
            cout << in << " ";
        cout << endl;
        for (const auto &proc : pair.second.procedures)
        {
            cout << "    " << proc.name << "  ";
            for (const auto &in : proc.ins)
                cout << in << " ";
            cout << "  ";
            for (const auto &out : proc.outs)
                cout << out << " ";
            cout << endl;
        }
        for (const auto &out : pair.second.outs)
            cout << out << " ";
        cout << endl
             << endl;
    }
}

string repeat(int n, string s)
{
    string result;
    for (int i = 0; i < n; ++i)
        result += s;
    return result;
}

void Flush()
{
    if (print.empty())
        return;
    if (print.size() % 2 != 0)
        print.push_back(vector<bool>{});
    for (int y = 0; y < print.size() - 1; y += 2)
    {
        const auto &r0 = print[y];
        const auto &r1 = print[y + 1];
        auto longest = max(r0.size(), r1.size());
        for (int x = 0; x < longest; ++x)
        {
            bool c0 = (x < r0.size() && r0[x]);
            bool c1 = (x < r1.size() && r1[x]);
            if (c0 && c1)
                cout << "█";
            else if (c0)
                cout << "▀";
            else if (c1)
                cout << "▄";
            else
                cout << " ";
        }
        cout << endl;
    }
    print.clear();
}
