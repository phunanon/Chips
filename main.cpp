#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <algorithm>
using namespace std;

struct Procedure
{
    string fileName;
    int line;
    string name;
    vector<string> ins;
    vector<string> outs;

    Procedure(string fileName, int line, string name, vector<string> ins, vector<string> outs)
        : fileName(fileName), line(line), name(name), ins(ins), outs(outs) {}
};

struct Chip
{
    string name;
    vector<string> ins;
    vector<string> outs;
    vector<Procedure> procedures;

    vector<bool> execute(const map<string, Chip> &chips, const vector<bool> &inputs, int debug) const;
};

#define THROW_SIGNAL_NOT_FOUND(signal)                                     \
    {                                                                      \
        cout << p.fileName << ":" << p.line + 1 << " available signals: "; \
        for (const auto &pair : signals)                                   \
            cout << pair.first << " ";                                     \
        cout << endl;                                                      \
        throw runtime_error("No signal \"" + signal + "\"");               \
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
#define COUT_SIGNAL(signal)                                                                                   \
    "\033[37;"                                                                                                \
        << ((signals.find(signal) != signals.end() && signals.at(signal)) || signal == "1" ? "42m " : "41m ") \
        << signal << " \033[0m"

string repeat(int n, string s);
vector<vector<bool>> print;
void Flush();

vector<bool> Chip::execute(const map<string, Chip> &chips, const vector<bool> &inputs, int debug) const
{
    map<string, bool> signals;

    if (ins.size() != inputs.size())
        throw runtime_error(name + ": arity mismatch, expected " + to_string(ins.size()) + ", got " + to_string(inputs.size()));
    for (size_t i = 0; i < ins.size(); ++i)
        signals[ins[i]] = inputs[i];

    if (debug)
    {
        cout << repeat(debug - 1, "│") << "┌╴" << name << " ";
        for (const auto &in : ins)
            cout << COUT_SIGNAL(in);
        cout << endl;
    }

    for (const auto &p : procedures)
    {
        if (p.name == "->")
        {
            if (p.ins.size() != p.outs.size())
                throw runtime_error(p.fileName + ":" + to_string(p.line + 1) + ": imbalanced");
            for (size_t j = 0; j < p.ins.size(); ++j)
            {
                RESOLVE_SIGNAL(p.ins[j], signals[p.outs[j]])
            }
        }
        else if (p.name == "AND" || p.name == "NAND" || p.name == "OR" || p.name == "NOR")
        {
            if (p.outs.size() != 1)
                throw runtime_error(p.fileName + ":" + to_string(p.line + 1) + ": invalid " + p.name);
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
                cout << repeat(debug - 1, "│") << "├╴" << p.name << string(5 - p.name.length(), ' ');
                cout << COUT_SIGNAL(p.ins[0]);
                cout << COUT_SIGNAL(p.ins[1]) << " ➤  ";
                cout << COUT_SIGNAL(p.outs[0]) << endl;
            }
        }
        else if (p.name == "PrintChar" || p.name == "Print8" || p.name == "PrintBits")
        {
            if (p.name != "PrintBits" && p.ins.size() != 8)
                throw runtime_error(p.fileName + ":" + to_string(p.line + 1) + ": needs 8 inputs");
            uint8_t byte = 0;
            for (size_t j = 0; j < p.ins.size(); ++j)
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
                    throw runtime_error(p.fileName + ":" + to_string(p.line + 1) + ": chip " + p.name + " not found");
                vector<bool> chipOutputs = chip->second.execute(chips, chipInputs, debug + !!debug);
                for (size_t j = 0; j < p.outs.size(); ++j)
                    signals[p.outs[j]] = chipOutputs[j];
            }
        }
    }
    vector<bool> outputs(outs.size());
    for (size_t i = 0; i < outs.size(); ++i)
        if (signals.find(outs[i]) != signals.end())
            outputs[i] = signals.at(outs[i]);
    if (debug)
    {
        cout << repeat(debug - 1, "│") << "└╴➤    ";
        vector<string> nonOuts;
        for (auto &s : signals)
            if (find(outs.begin(), outs.end(), s.first) == outs.end())
                nonOuts.push_back(s.first);
        for (const auto &out : outs)
            cout << COUT_SIGNAL(out);
        cout << "  ";
        for (const auto &nonOut : nonOuts)
            cout << COUT_SIGNAL(nonOut);
        cout << endl;
    }
    return outputs;
}

void EchoChips(const map<string, Chip> &chips);

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

void Main(unordered_set<string> &files, unordered_set<string> &flags, vector<bool> &argsForMain)
{
    vector<tuple<string, string>> lines;
    for (const auto &fileName : files)
    {
        auto fileLines = readFileToVector(fileName);
        for (const auto &line : fileLines)
        {
            lines.emplace_back(fileName, line);
        }
        lines.emplace_back(fileName, "");
    }

    map<string, Chip> chips;
    bool newChip = true;
    bool readInps = false;
    bool readProc = false;
    bool readOuts = false;
    string chipName, procName;
    for (int l = 0; l < lines.size(); ++l)
    {
        string fileName = get<0>(lines[l]);
        string line = get<1>(lines[l]);
        if (line[0] == '#')
            continue;
        string nextLine = (l + 1 < lines.size()) ? get<1>(lines[l + 1]) : "";
        if (line == "")
        {
            newChip = true;
            readOuts = false;
        }
        else if (newChip)
        {
            chipName = line;
            chips[chipName] = Chip{chipName, {}, {}, {}};
            newChip = false;
            auto doReadInps = nextLine[0] != ' ';
            readInps = doReadInps;
            readProc = !doReadInps;
        }
        else if (readInps || readOuts)
        {
            stringstream ss(line);
            string chunk;
            while (getline(ss, chunk, ' '))
                if (chunk != "")
                    if (readInps)
                        chips[chipName].ins.push_back(chunk);
                    else if (readOuts)
                        chips[chipName].outs.push_back(chunk);
            if (nextLine[0] == ' ' || nextLine[0] == '\0')
            {
                readProc = readInps;
                readInps = false;
                readOuts = false;
            }
        }
        else if (readProc)
        {
            vector<string> procIns, procOuts;
            string thisProcName = "";
            bool readProcInps = false;
            bool readProcOuts = false;
            stringstream ss(line);
            string chunk;
            while (getline(ss, chunk, ' '))
            {
                if (chunk == "")
                {
                    if (procIns.size() && readProcInps)
                    {
                        readProcInps = false;
                        readProcOuts = true;
                    }
                }
                else if (thisProcName.empty())
                {
                    if (chunk != "\"")
                        procName = chunk;
                    thisProcName = procName;
                    readProcInps = true;
                }
                else if (readProcInps)
                {
                    procIns.push_back(chunk);
                }
                else if (readProcOuts)
                {
                    procOuts.push_back(chunk);
                }
            }
            chips[chipName].procedures.push_back({fileName, l, thisProcName, procIns, procOuts});
            if (nextLine[0] != ' ' && nextLine[0] != '#')
            {
                readProc = false;
                readOuts = true;
            }
        }
    }

    auto debug = flags.count("--debug");
    auto echoChips = flags.count("--echo-chips");
    if (echoChips)
        EchoChips(chips);

    if (chips.find("Main") == chips.end())
    {
        cout << "Chip \"Main\" not found." << endl;
    }
    else
    {
        chips["Main"].execute(chips, argsForMain, debug);
    }
}

int main(int argc, char **argv)
{
    vector<string> args(argv + 1, argv + argc);
    unordered_set<string> files;
    unordered_set<string> flags;
    vector<bool> argsForMain;

    for (const auto &arg : args)
        if (arg[0] == '-')
            flags.insert(arg);
        else if (all_of(arg.begin(), arg.end(), [](const char &s)
                        { return s == '0' || s == '1'; }))
            for (const auto &c : arg)
                argsForMain.push_back(c == '1');
        else
            files.insert(arg);

    if (!files.size())
        throw runtime_error("Usage: " + string(argv[0]) + " <filenames> [--echo-file] [--debug]");

    Main(files, flags, argsForMain);

    return 0;
}

void EchoChips(const map<string, Chip> &chips)
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
